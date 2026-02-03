#pragma once

#include "Request.hpp"
#include <string>
#include <map>
#include <vector>
#include <utility>
#include <sys/types.h>

struct CgiResponse
{
    std::string status = "200 OK";
    std::string contentType = "text/plain";
    std::string contentLength = "0";
    std::string body;
};

class CgiHandler
{
public:
    static std::pair<pid_t, int> execute(const std::string &scriptPath, const Request &request);
    static CgiResponse parseCgiOutput(const std::string &rawOutput);

private:
    static std::map<std::string, std::string> getEnv(const std::string &scriptPath, const Request &request);
    static char **mapToEnvp(const std::map<std::string, std::string> &envMap);
};