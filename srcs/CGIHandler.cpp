#include  "CGIHandler.hpp"

std::map<std::string, std::string> CGIHandler::getEnv(const std::string& scriptPath, const Request& request) {
    std::map<std::string, std::string> env;

    env["REQUEST_METHOD"]   = (request.getRequestMethod() == RequestMethod::Post ? "POST" : (request.getRequestMethod() == RequestMethod::Get ? "GET" : "UNKNOWN"));
    env["QUERY_STRING"]     = request.getQuery().has_value() ? request.getQuery().value() : " ";
    env["CONTENT_LENGTH"]   = std::to_string(request.getBody().size());
    env["PATH_INFO"]        = request.getTarget();   
    env["SCRIPT_FILENAME"]  = scriptPath;

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

    pid_t pid = fork();

    if(pid == -1) {
        ERROR_LOG("CGI fork failed!");
        close(pipe_in[0]);close(pipe_in[1]);
        close(pipe_out[0]);close(pipe_out[1]);
        return ("Status: 500 Internal Server Error\r\n\r\n");
    }

    if(pid == 0) {
        // insdie child process

    } else {
        // inside parent process
    }
    (void) request;
    return (scriptPath);
}