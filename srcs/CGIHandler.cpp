#include  "CGIHandler.hpp"

void freeEnvp(char** envp) {
    if (!envp) return;
    for (int i = 0; envp[i] != nullptr; i++) {
        delete[] envp[i]; // Delete individual strings
    }
    delete[] envp; // Delete the array of pointers
}

std::map<std::string, std::string> CGIHandler::getEnv(const std::string& scriptPath, const Request& request) {
    std::map<std::string, std::string> env;

    env["REQUEST_METHOD"]       = (request.getRequestMethod() == RequestMethod::Post ? "POST" : (request.getRequestMethod() == RequestMethod::Get ? "GET" : "UNKNOWN"));
    env["QUERY_STRING"]         = request.getQuery().has_value() ? request.getQuery().value() : "";
    env["CONTENT_LENGTH"]       = std::to_string(request.getBody().size());
    env["PATH_INFO"]            = request.getTarget();   
    env["SCRIPT_FILENAME"]      = scriptPath;
    env["GATEWAY_INTERFACE"]    = "CGI/1.1";
    env["SERVER_PROTOCOL"]      = request.getHttpVersion();
    env["SERVER_NAME"]          = request.getHost();
    env["SERVER_SOFTWARE"]      = "webserv 1.0";

    auto ct = request.getHeader("content-type");
    if(ct && !ct->empty()) {
        env["CONTENT_TYPE"] = ct->front();
    }
    
    return (env);
}

char** CGIHandler::mapToEnvp(const std::map<std::string, std::string>& envMap) {
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

std::string CGIHandler::execute(const std::string& scriptPath, const Request& request) {
    int pipe_in[2];
    int pipe_out[2];

    if(pipe(pipe_in) == -1 || pipe(pipe_out) == -1) {
        ERROR_LOG("CGI Pipe failed!");
        return ("Status: 500 Internal Server Error\r\n\r\n");
    }

    // preparing enviornment and args for execv
    std::map<std::string, std::string> envMap = getEnv(scriptPath, request);
    char** envp = mapToEnvp(envMap);
    char* argv[] = { const_cast<char* >(scriptPath.c_str()), nullptr };

    pid_t pid = fork();

    if(pid == -1) {
        ERROR_LOG("CGI fork failed!");
        close(pipe_in[0]);close(pipe_in[1]);
        close(pipe_out[0]);close(pipe_out[1]);
        freeEnvp(envp);
        return ("Status: 500 Internal Server Error\r\n\r\n");
    }

    if(pid == 0) {
        // insdie child process

        // reading from the STDIN_FILENO
        dup2(pipe_in[0], STDIN_FILENO);
        // writing to the STDOUT_FILENO
        dup2(pipe_out[1], STDOUT_FILENO);

        // closing unused file FDs
        close(pipe_in[0]);close(pipe_in[1]);
        close(pipe_out[0]);close(pipe_out[1]);

        // execute script
        execve(scriptPath.c_str(), argv, envp);

        // <-- from here, if execve fails -->
        std::cerr << "CGI execve failed: " << strerror(errno) << std::endl;
        exit(1);

    } else {
        // inside parent process

        // closing read end and write end
        close(pipe_in[0]); close(pipe_out[1]);
        // make read-edn non-blocking
        fcntl(pipe_out[0], F_SETFL, O_NONBLOCK);

        if(!request.getBody().empty()) {
            write(pipe_in[1], request.getBody().c_str(), request.getBody().size());
        }
        close(pipe_in[1]);

        std::string result;     // variable to store the result
        char buffer[4096];      // temporay buffer
        ssize_t bytesRead;      // track how much data read
        int status;             // variable to track the status
        int timeOut = 5;        // defauls time-out to avoid infinite loops

        // starting the clock to track the time-out
        auto start = std::chrono::steady_clock::now();

        while(true) {

            int wait_result = waitpid(pid, &status, WNOHANG);

            while((bytesRead = read(pipe_out[0], buffer, sizeof(buffer))) > 0) {
                result.append(buffer, bytesRead);
            }

            if(wait_result == pid) {
                // child running successfull
                break;
            } else if (wait_result == 0) {
                // child still running, check for timeout 
                auto now = std::chrono::steady_clock::now();

                if(std::chrono::duration_cast<std::chrono::seconds>(now - start).count() >= timeOut) {
                    kill(pid, SIGKILL);
                    waitpid(pid, &status, 0);
                    result = "Status: 504 Gateway Timeout\r\n\r\n";
                    break;
                }
                // wait 5ms before next check
                usleep(5000); 
            } else {
                // something went wrong
                result = "Status: 500 Internal Server Error\r\n\r\n";
                break;
            }
        }
        
        close(pipe_out[0]);
        freeEnvp(envp);
        return (result);
    }
    return ("Status: 500 Internal Server Error\r\n\r\n");
}