#pragma once

#include "Request.hpp"
#include <string>
#include <map>
#include <vector>
#include <utility>
#include <sys/types.h>

class CgiHandler
{
public:
    static std::pair<pid_t, int> execute(const std::string &scriptPath, const Request &request);

private:
    static std::map<std::string, std::string> getEnv(const std::string &scriptPath, const Request &request);
    static char **mapToEnvp(const std::map<std::string, std::string> &envMap);
};