#include "CgiHandler.hpp"
#include "Utils.hpp"
#include "Log.hpp"
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#define READ	0
#define WRITE	1

void	freeEnvp(char** envp)
{
	if (!envp)
		return;
	for (int i = 0; envp[i] != nullptr; i++)
		delete[] envp[i];
	delete[] envp;
}

std::map<std::string, std::string>	CgiHandler::getEnv(std::string const &scriptPath, Request const &request, Config const &conf)
{
	stringMap	env;

	// META_VARIABLES required by the RFC 3875
	switch (request.getRequestMethod()) {
		case RequestMethod::Post:	env["REQUEST_METHOD"] = "POST";		break;
		case RequestMethod::Get:	env["REQUEST_METHOD"] = "GET";		break;
		default:					env["REQUEST_METHOD"] = "UNKNOWN";	break;
	}
	env["QUERY_STRING"]			= request.getQuery().has_value() ? request.getQuery().value() : "";
	env["CONTENT_LENGTH"]		= std::to_string(request.getBody().length());
	env["PATH_INFO"]			= request.getTarget();
	env["SCRIPT_FILENAME"]		= scriptPath;
	env["GATEWAY_INTERFACE"]	= "CGI/1.1";
	env["SCRIPT_NAME"]			= request.getTarget();
	env["REQUEST_URI"]			= request.getTarget();
	env["SERVER_PROTOCOL"]		= request.getHttpVersion();
	env["SERVER_SOFTWARE"]		= "Webserv/1.0";
	env["REDIRECT_STATUS"]		= "200";

	auto	ct = request.getHeader("content-type");

	if (ct && !ct->empty())
		env["CONTENT_TYPE"] = ct->front();

	// SERVER_DATA required by the RFC 3875
	env["SERVER_NAME"] = conf.host;
	env["SERVER_PORT"] = conf.port;

	// SCHEME_DATA required by RFC 3875
	for (auto const &[key, values] : request.getHeaders()) {
		if (values.empty())
			continue;

		std::string	envKey = "HTTP_";

		for (unsigned char const c : key)
			envKey += (c == '-' ? '_' : std::toupper(c));
		env[envKey] = values.front();
	}

	return (env);
}

char	**CgiHandler::mapToEnvp(std::map<std::string, std::string> const &envMap)
{
	char	**envp	= new char*[envMap.size() + 1];
	size_t	i		= 0;

	for (auto const &[key, value] : envMap) {
		std::string	temp = key + "=" + value;
		char		*str = new char[temp.size() + 1];

		std::strcpy(str, temp.c_str());
		envp[i++] = str;
	}
	envp[i] = nullptr;

	return (envp);
}

std::pair<pid_t, int>	CgiHandler::execute(std::string const &scriptPath, Request const &request, Config const &conf)
{
	int	parentToChildPipe[2];
	int	childToParentPipe[2];

	if (pipe(parentToChildPipe) == -1 || pipe(childToParentPipe) == -1) {
		ERROR_LOG("CGI Pipe failed, client fd " + std::to_string(request.getFd()));
		// Clean up parentToChildPipe if childToParentPipe failed
		if (parentToChildPipe[READ] != -1) {
			close(parentToChildPipe[READ]);
			close(parentToChildPipe[WRITE]);
		}
		return {-1, -1};
	}

	stringMap	envMap	= getEnv(scriptPath, request, conf);
	std::string path	= scriptPath; // Making copy for persisting existence

	char	**envp	= mapToEnvp(envMap);
	char	*argv[]	= { const_cast<char*>(path.c_str()), nullptr };

	pid_t	pid = fork();

	if (pid == -1) {
		ERROR_LOG("CGI fork failed, client fd " + std::to_string(request.getFd()));
		close(parentToChildPipe[READ]);
		close(parentToChildPipe[WRITE]);
		close(childToParentPipe[READ]);
		close(childToParentPipe[WRITE]);
		freeEnvp(envp);
		return {-1, -1};
	}

	if (pid == 0) {
		// Inside child process

		// Redirect stdin to read from parentToChildPipe
		if (dup2(parentToChildPipe[READ], STDIN_FILENO) == -1) {
			ERROR_LOG("CGI dup2 input failed: " + std::string(strerror(errno))
				+ ", client fd " + std::to_string(request.getFd()));
			std::exit(1);
		}
		// Redirect stdout to write to childToParentPipe
		if (dup2(childToParentPipe[WRITE], STDOUT_FILENO) == -1) {
			ERROR_LOG("CGI dup2 output failed: " + std::string(strerror(errno))
				+ ", client fd " + std::to_string(request.getFd()));
			std::exit(1);
		}

		// Closing unused file fds
		close(parentToChildPipe[READ]);
		close(parentToChildPipe[WRITE]);
		close(childToParentPipe[READ]);
		close(childToParentPipe[WRITE]);

		// Execute script
		execve(path.c_str(), argv, envp);

		// Execve fail fallback
		ERROR_LOG("CGI execve failed: " + std::string(strerror(errno))
			+ ", client fd " + std::to_string(request.getFd()));
		std::exit(1);
	}

	// Inside parent process
	close(parentToChildPipe[READ]);
	close(childToParentPipe[WRITE]);

	// Make read-end non-blocking
	if (fcntl(childToParentPipe[READ], F_SETFL, O_NONBLOCK) == -1) {
		ERROR_LOG("CGI fcntl failed: " + std::string(strerror(errno))
			+ ", client fd " + std::to_string(request.getFd()));
		close(parentToChildPipe[WRITE]);
		close(childToParentPipe[READ]);
		freeEnvp(envp);
		kill(pid, SIGKILL);
		waitpid(pid, nullptr, 0);
		return {-1, -1};
	}

	// Pass request body data to child process
	if (!request.getBody().empty()) {
		char const	*data		= request.getBody().c_str();
		size_t		remaining	= request.getBody().length();

		while (remaining > 0) {
			ssize_t	written = write(parentToChildPipe[WRITE], data, remaining);

			if (written == -1) {
				ERROR_LOG("CGI write failed: " + std::string(strerror(errno))
					+ ", client fd " + std::to_string(request.getFd()));
				break;
			}
			data += written;
			remaining -= written;
		}
	}
	close(parentToChildPipe[WRITE]);
	freeEnvp(envp);

	return { pid, childToParentPipe[READ] };
}

CgiResponse	CgiHandler::parseCgiOutput(std::string const &rawOutput)
{
	CgiResponse	response;

	if (rawOutput.empty()) {
		response.badCgiOutput	= true;
		return response;
	}

	// Separate header section from body section
	size_t	bodyPos			= rawOutput.find("\r\n\r\n");
	size_t	headerEndLen	= 4;

	if (bodyPos == std::string::npos) {
		bodyPos = rawOutput.find("\r\n");
		headerEndLen = 2;
	}

	// Handle case where no header/body separator is found
	if (bodyPos == std::string::npos) {
		response.body = rawOutput;

		return response;
	}

	std::string	headerSection = rawOutput.substr(0, bodyPos);

	response.body = rawOutput.substr(bodyPos + headerEndLen);

	std::string	line;
	size_t		start = 0;
	size_t		end;

	while ((end = headerSection.find('\n', start)) != std::string::npos) {
		line = headerSection.substr(start, end - start);
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		size_t	colonPos = line.find(':');

		if (colonPos != std::string::npos) {
			std::string key		= line.substr(0, colonPos);
			std::string value	= line.substr(colonPos + 1);

			// Trim leading/trailing spaces from value
			size_t	first	= value.find_first_not_of(" ");
			size_t	last	= value.find_last_not_of(" ");

			if (first != std::string::npos)
				value = value.substr(first, (last - first + 1));

			// Mapping fields
			if (key == "Status"){
				response.statusString = value;
				try {
					std::string	trimmed	= trimWhitespace(value);
					size_t		pos		= trimmed.find_first_not_of("0123456789");

					response.status = std::stoi(trimmed.substr(0, pos));
				} catch (std::exception &e) {
					response.badCgiOutput = true;
					break;
				}
			}
			else if (key == "Content-Type")
				response.contentType = value;
			else if (key == "Content-Length") {
				try {
					size_t	contentLength = std::stoi(value);

					if (contentLength != response.body.length())
						response.badCgiOutput = true;
				} catch (std::exception &e) {
					response.badCgiOutput = true;
				}
			}
		}
		start = end + 1;
	}

	return response;
}
