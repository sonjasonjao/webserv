#pragma once

#include "CustomException.hpp"
#include "JSON.hpp"
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>

/**
 * Default extension for configuration files. Files without the extension are
 * considered invalid.
 */
#define EXTENSION "json"

/**
 * Helper struct for server routes, aka mapping specific request targets to
 * internal folders on the host running the server.
 */
struct Route {
	std::string								original;
	std::string								target;
	std::optional<std::vector<std::string>>	allowedMethods;
};

/**
 * Struct for holding server configuration data
 */
struct Config {
	std::string	host;		// IP on which this server listens, e.g. "0.0.0.0", "127.0.0.1" or "localhost"
	std::string	serverName;	// Assigned name of server in config file, e.g. "localhost" or "www.example.com"

	std::optional<std::string>	uploadDir;	// Upload directory path, uploads disabled if left unset

	uint16_t	port = 0;	// Listening socket port linked to server configuration

	std::map<std::string, std::string>	statusPages;	// Mapping from HTTP status code to custom page path
	std::map<std::string, Route>		routes;			// Set of routes (URI -> internal host path) for this server

	std::optional<std::vector<std::string>>	allowedMethods;	// Server level allowed HTTP request methods

	std::optional<size_t>	clientMaxBodySize;	// Default maximum allowed size (in bytes) of the request body for this server

	bool	directoryListing	= false;
	bool	autoindex			= false;
};

class Parser {
private:
	std::string const	_fileName;			// File name of the configuration file
	std::ifstream		_file;				// ifstream instance to read the configuration file
	std::vector<Config>	_serverConfigs;		// List of fully parsed server configurations built from the token list

public:
	Parser(std::string const &fileName);
	Parser() = delete;
	Parser(Parser const &other) = delete;
	~Parser();

	Parser	&operator=(Parser const &other) = delete;

	struct ParserException : public CustomException {
		ParserException(std::string const &str):
			CustomException(str) {}
	};

	void tokenizeFile();

	Config const				&getServerConfig(size_t index) const;
	std::vector<Config> const	&getServerConfigs() const;
	std::vector<std::string>	getCollectionBykey(Token const &root, std::string const &key);
	size_t						getNumberOfServerConfigs();

	Config	convertToServerData(Token const &server);

	bool	isValidJSONString(std::string_view sv);
	bool	isPrimitiveValue(std::string_view sv);
};
