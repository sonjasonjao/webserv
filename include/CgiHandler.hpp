#pragma once

#include "Request.hpp"
#include "Utils.hpp"
#include <string>
#include <map>
#include <utility>
#include <sys/types.h>

struct CgiResponse {
	int	status					= 200;
	int	contentLength			= 0;
	std::string	statusString	= "200 OK";
    std::string contentType		= "text/html";
	std::string	body;
};

class CgiHandler {
	using stringMap = std::map<std::string, std::string>;

public:
	static std::pair<pid_t, int>	execute(std::string const &scriptPath, Request const &request);
	static CgiResponse				parseCgiOutput(std::string const &rawOutput);

private:
	static stringMap	getEnv(std::string const &scriptPath, Request const &request);
	static char			**mapToEnvp(std::map<std::string, std::string> const &envMap);
};
