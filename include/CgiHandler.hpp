#pragma once

#include "Request.hpp"
#include <string>
#include <map>
#include <utility>
#include <sys/types.h>

struct CgiResponse {
	int	status					= 0;
	int	contentLength			= 0;
	bool isSucceeded			= true;
	std::string	statusString	= "default";
    std::string contentType		= "default";
	std::string	body;
};

class CgiHandler {

	using stringMap = std::map<std::string, std::string>;

public:
	static std::pair<pid_t, int>	execute(std::string const &scriptPath, Request const &request, Config const &conf);
	static CgiResponse				parseCgiOutput(std::string const &rawOutput);

private:
	static stringMap	getEnv(std::string const &scriptPath, Request const &request, Config const &conf);
	static char			**mapToEnvp(std::map<std::string, std::string> const &envMap);
};
