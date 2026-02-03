#include "CgiHandler.hpp"
#include "Log.hpp"
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

void freeEnvp(char** envp) {
    if (!envp) return;
    for (int i = 0; envp[i] != nullptr; i++) {
        delete[] envp[i]; // Delete individual strings
    }
    delete[] envp; // Delete the array of pointers
}

std::map<std::string, std::string> CgiHandler::getEnv(const std::string& scriptPath, const Request& request) {
    std::map<std::string, std::string> env;

    // META_VARIABLES required by the RFC 3875
    env["REQUEST_METHOD"]       = (request.getRequestMethod() == RequestMethod::Post ? "POST" : (request.getRequestMethod() == RequestMethod::Get ? "GET" : "UNKNOWN"));
    env["QUERY_STRING"]         = request.getQuery().has_value() ? request.getQuery().value() : "";
    env["CONTENT_LENGTH"]       = std::to_string(request.getBody().size());
    env["PATH_INFO"]            = request.getTarget();   
    env["SCRIPT_FILENAME"]      = scriptPath;
    env["GATEWAY_INTERFACE"]    = "CGI/1.1";
    env["SCRIPT_NAME"] = request.getTarget();
    env["SERVER_PROTOCOL"]      = request.getHttpVersion();
    env["SERVER_SOFTWARE"]      = "Webserv/1.0";
    env["REDIRECT_STATUS"]      = "200"; // this was added in one of our senior's project for php

    auto ct = request.getHeader("content-type");
    if(ct && !ct->empty()) {
        env["CONTENT_TYPE"] = ct->front();
    }

    // SERVER_DATA required by the RFC 3875
    std::string host = request.getHost();
    size_t colon = host.find(':');
    if (colon != std::string::npos) {
        env["SERVER_NAME"] = host.substr(0, colon);
        env["SERVER_PORT"] = host.substr(colon + 1);
    } else {
        env["SERVER_NAME"] = host;
        env["SERVER_PORT"] = "8080"; // we can change this to anything
    }

    // SCHEME_DATA required by RFC 3875 
    for (const auto& [key, values] : request.getHeaders()) {
        if (values.empty()) continue;
        std::string envKey = "HTTP_";
        for (char c : key) {
            envKey += (c == '-' ? '_' : std::toupper(c));
        }
        env[envKey] = values.front(); 
    }

    return (env);
}

char** CgiHandler::mapToEnvp(const std::map<std::string, std::string>& envMap) {
    char** envp = new char* [envMap.size() + 1];

    int i = 0;
    for(const auto& [key, value] : envMap) {
        std::string temp = key + "=" + value;
        char* str = new char[temp.size() + 1];
        std::strcpy(str, temp.c_str());
        envp[i] = str;
        i++;
    }

    envp[i] = nullptr;
    return (envp);
}

std::pair<pid_t, int> CgiHandler::execute(const std::string& scriptPath, const Request& request) {
    
    int pipe_in[2];
    int pipe_out[2];

    if(pipe(pipe_in) == -1 || pipe(pipe_out) == -1) {
        ERROR_LOG("CGI Pipe failed!");
        // Clean up pipe_in if pipe_out failed
        if (pipe_in[0] != -1)
        {
            close(pipe_in[0]);
            close(pipe_in[1]);
        }
        return {-1, -1};
    }
    std::map<std::string, std::string> envMap = getEnv(scriptPath, request);
    // making copy for persisting existance
    std::string path = scriptPath;

    char** envp = mapToEnvp(envMap);
    char* argv[] = { const_cast<char* >(path.c_str()), nullptr };

    pid_t pid = fork();

    if(pid == -1) {
        ERROR_LOG("CGI fork failed!");
        close(pipe_in[0]);close(pipe_in[1]);
        close(pipe_out[0]);close(pipe_out[1]);
        freeEnvp(envp);
        return {-1, -1};
    }

    if(pid == 0) {
        // insdie child process
        // Redirect stdin to read from pipe_in
        if (dup2(pipe_in[0], STDOUT_FILENO) == -1)
        {
            ERROR_LOG("CGI dup2 input failed: " + std::string(strerror(errno)));
            std::exit(1);
        }
        // Redirect stdout to write to pipe_out
        if (dup2(pipe_out[1], STDOUT_FILENO) == -1 ) {
            ERROR_LOG("CGI dup2 output failed: " + std::string(strerror(errno)));
            std::exit(1);
        }

        // closing unused file FDs
        close(pipe_in[0]);close(pipe_in[1]);
        close(pipe_out[0]);close(pipe_out[1]);

        // execute script
        execve(path.c_str(), argv, envp);

        // <-- from here, if execve fails -->
        std::cerr << "CGI execve failed: " << strerror(errno) << std::endl;
        std::exit(1);

    } else {
        // inside parent process

        // closing read end and write end
        close(pipe_in[0]); close(pipe_out[1]);
        
        // make read-end non-blocking
        if(fcntl(pipe_out[0], F_SETFL, O_NONBLOCK) == -1) { 
            ERROR_LOG("CGI fcntl failed: " + std::string(strerror(errno)));
            close(pipe_in[1]); close(pipe_out[0]);
            kill(pid, SIGKILL);
            waitpid(pid, nullptr, 0);
            return {-1, -1};
        }

        // pass request body data to CHILD process
        if (!request.getBody().empty())
        {
            const char *data = request.getBody().c_str();
            size_t remaining = request.getBody().size();
            while (remaining > 0)
            {
                ssize_t written = write(pipe_in[1], data, remaining);
                if (written == -1)
                {
                    if (errno == EINTR)
                        continue;
                    ERROR_LOG("CGI write failed: " + std::string(strerror(errno)));
                    break;
                }
                data += written;
                remaining -= written;
            }
        }
        close(pipe_in[1]);
        freeEnvp(envp);
        return { pid, pipe_out[0] };
    }
    return { -1, -1 };
}

CgiResponse CgiHandler::parseCgiOutput(const std::string &rawOutput)
{
    CgiResponse response;

    // Separate Header section from Body section
    size_t bodyPos = rawOutput.find("\r\n\r\n");
    size_t headerEndLen = 4;

    if (bodyPos == std::string::npos)
    {
        bodyPos = rawOutput.find("\n\n");
        headerEndLen = 2;
    }

    // Handle case where no header/body separator is found
    if (bodyPos == std::string::npos)
    {
        response.body = rawOutput;
        response.contentLength = std::to_string(response.body.length());
        return response;
    }

    std::string headerSection = rawOutput.substr(0, bodyPos);
    response.body = rawOutput.substr(bodyPos + headerEndLen);

    std::string line;
    size_t start = 0;
    size_t end;

    while ((end = headerSection.find('\n', start)) != std::string::npos)
    {
        line = headerSection.substr(start, end - start);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos)
        {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);

            // Trim leading/trailing spaces from value
            size_t first = value.find_first_not_of(" ");
            size_t last = value.find_last_not_of(" ");
            if (first != std::string::npos)
            {
                value = value.substr(first, (last - first + 1));
            }

            // Mapping fields
            if (key == "Status")
                response.status = value;
            else if (key == "Content-Type")
                response.contentType = value;
            else if (key == "Content-Length")
                response.contentLength = value;
        }
        start = end + 1;
    }

    // Content-Length, set it based on body size
    if (response.contentLength == "0" && !response.body.empty())
    {
        response.contentLength = std::to_string(response.body.length());
    }

    return response;
}