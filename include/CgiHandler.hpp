#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstring>
#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

#include "Request.hpp"
#include "Utils.hpp"
#include "Log.hpp"

class CgiHandler {
    public:
        static std::pair<pid_t, int> execute(const std::string& scriptPath, const Request& request);
    private:
        static std::map<std::string, std::string> getEnv(const std::string& scriptPath, const Request& request);
        static char** mapToEnvp(const std::map<std::string, std::string>& envMap);
};