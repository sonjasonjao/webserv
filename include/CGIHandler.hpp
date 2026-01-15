#pragma once


#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <cstdint>

#include "Utils.hpp"

/**
 * this part starting from here is only for tempprary purpose will remove this eventually 
 */
enum class METHOD {
    GET,
    POST,
    DELETE,
    PUT,
    HEAD,
    UNKNOWN
};

enum class VERSION {
    POINT_NINE,
    ONE_POINT_ZERO,
    ONE_POINT_ONE,
    UNKNOWN
};

struct RequestData {
    METHOD      http_method;
    VERSION     http_version;
    std::string path;
    std::string query_string;
    std::string host;
    std::string content_type;
    size_t      content_length;
    std::string body;
    bool        is_keep_alive;
    bool        is_valid;

    RequestData() {
        http_method     = METHOD::UNKNOWN;
        http_version    = VERSION::UNKNOWN;
        path = "";
        query_string = "";
        host = "";
        content_type = "";
        content_length = 0;
        body = "";
        is_keep_alive = false;
        is_valid      = false;
    }
};

struct ResponseData {
    uint16_t            status_code;
    std::string         content_type;
    std::vector<char>   buffer;

    ResponseData() {
        status_code  = 0;
        content_type = "";
    }
};

struct Client {
    size_t          client_Fd;
    bool            is_complete;
    std::string     request_buffer;
    std::string     response_buffer;
    size_t          size_sent;
    size_t          size_recv;
    ResponseData    response;
    RequestData     request;

    Client(size_t newFD) : client_Fd(newFD), 
        is_complete(false), 
        request_buffer(""),
        response_buffer(""),
        size_sent(0),
        size_recv(0),
        response (),
        request  () {
    }

    void addToRequestBuffer(std::string str) {
        request_buffer.append(str);
        if(request_buffer.find("\r\n\r\n") != std::string::npos){
            is_complete = true;
        }
    }

    void clearAndUpdateRequestBuffer(std::string str) {
        request_buffer.clear();
        is_complete = false;
        addToRequestBuffer(str);
    }

    void addToResponseBuffer(std::string str) {
        response_buffer.append(str);
    }

    void clearAndUpdateResponseBuffer(std::string str) {
        response.status_code = 0;
        response_buffer.clear();
        addToResponseBuffer(str);
    }

    bool isResponseCompleted(void) {
        if (size_sent >= response_buffer.size()) {
            return (true);
        }
        return (false);
    }
};


/**
 * end of temporary part
 * ###################################################################################### 
 */

class CGIHandler {
    public:
        static std::string execute(const std::string& scriptPath, const Client& client) {
        
            int pipe_in[2];  // Parent -> Child (Write body)
            int pipe_out[2]; // Child -> Parent (Read HTML)

            if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1){
                return "Error: Pipe failed";
            } 

            pid_t pid = fork();
            if (pid == -1) {
                return "Error: Fork failed";
            }

            if (pid == 0) { // child process
            
            // read from the STDIN
            dup2(pipe_in[0], STDIN_FILENO);
            // wite to STDOUT
            dup2(pipe_out[1], STDOUT_FILENO);
            
            close(pipe_in[0]); 
            close(pipe_in[1]);
            close(pipe_out[0]);
            close(pipe_out[1]);

            std::vector<std::string> env;
            env.push_back("REQUEST_METHOD=" + (client.request.http_method == METHOD::POST ? std::string("POST") : std::string("GET")));
            env.push_back("CONTENT_LENGTH=" + std::to_string(client.request.content_length));
            env.push_back("QUERY_STRING=" + client.request.query_string);
            env.push_back("CONTENT_TYPE=" + client.request.content_type);
            env.push_back("PATH_INFO=" + client.request.path);
            env.push_back("SCRIPT_FILENAME=" + scriptPath);

            std::vector<char*> envp;
            for (auto& s : env) envp.push_back(const_cast<char*>(s.c_str()));
            envp.push_back(nullptr);

            char* argv[] = { const_cast<char*>(scriptPath.c_str()), nullptr };

            // execute the script 
            execve(scriptPath.c_str(), argv, envp.data());
            exit(1);

        } else { // parent process
            close(pipe_in[0]);
            
            // write body to the child process
            if (client.request.http_method == METHOD::POST && !client.request.body.empty()) {
                write(pipe_in[1], client.request.body.c_str(), client.request.body.size());
            }

            close(pipe_in[1]);
            close(pipe_out[1]);

            // read output
            std::string result;
            char buffer[4096];
            ssize_t bytesRead;
            while ((bytesRead = read(pipe_out[0], buffer, sizeof(buffer))) > 0) {
                result.append(buffer, bytesRead);
            }
            close(pipe_out[0]);
            
            // zombie cleanup 
            waitpid(pid, nullptr, 0); 
            return (result);
        }
    }
};