#pragma once

#include "Request.hpp"
#include <string>
#include <map>
#include <utility>
#include <sys/types.h>

struct CgiResponse {
	std::string	contentType		= "text/plain";
	std::string	statusString;
	std::string	body;
	int			status			= 200;
	bool		badCgiOutput	= false;
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
