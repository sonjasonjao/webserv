#pragma once

#include <fstream>
#include <cstdint>
#include <cctype>
#include <string>
#include <vector>
#include <map>
#include <optional>

#include "CustomException.hpp"
#include "JSON.hpp"

/**
 * Default file_name extension for the configuration file, with out correct extension
 * file_name invalid.
 */
#define EXTENSION "json"

struct Route {
	std::string								target;
	std::optional<std::vector<std::string>>	allowedMethods	= std::nullopt;
};

struct Config {
	std::string	host;		// IP on which this server listens, e.g. "0.0.0.0", "127.0.0.1" or "localhost"
	std::string	serverName;	// Assigned name of server in config file, e.g. "localhost" or "www.example.com"
	std::string	uploadDir;	// Activates or deactivates upload directory behavior

	uint16_t	port = 0;	// Port on which this server listens

	std::map<std::string, std::string>	statusPages;	// Mapping from HTTP status code to custom page path.
	std::map<std::string, Route>		routes;			// Set of routes (URI -> path definitions) for this server.

	size_t	requestMaxBodySize	= 0;	// in bytes

	bool	directoryListing	= false;
	bool	autoindex			= false;
};

class Parser {
private:
	std::string const	_file_name;			// File name of the configuration file
	std::ifstream		_file;				// ifstream instance to read the configuration file
	std::vector<Config>	_server_configs;	// List of fully parsed server configurations built from the token list.

public:
	/**
	 * proper-way of creating Parser instance
	 * @param std::string file_name - file name need to parse
	 * @return void - content of the file will be tokenize and save to a internal
	 * container, type std::vector
	 */
	Parser(const std::string& file_name);
	/**
	 * The only way of creating a Parser instance should be via the argument constructor.
	 */
	Parser() = delete;
	Parser(const Parser& other) = delete;
	Parser& operator=(const Parser& other) = delete;
	/**
	 * destructor will take care of releasing resources
	 */
	~Parser();

	/**
	 * All the exceptions in Parsing will be categorize under ParserException
	 * and will carry a string describing what went wrong
	 */
	struct ParserException : public CustomException {
		ParserException(const std::string& str):
			CustomException(str) {}
	};

	/**
	 * Correct configuration file will be read line by line, every line will be tokenize
	 * and saved to the _tokens
	 * @param void
	 * @return void
	 */
	void tokenizeFile(void);

	/**
	 * First version of getter method to get a final server configuration information. As the first
	 * version only return some dummy values to start implementing Main loop for Sonja
	 */
	const Config&				getServerConfig(size_t index);
	const std::vector<Config>	&getServerConfigs(void) const;
	std::vector<std::string>	getCollectionBykey(const Token& root, const std::string& key);
	size_t						getNumberOfServerConfigs(void);

	Config	convertToServerData(const Token& server);

	bool	isValidJSONString(std::string_view sv);
	bool	isPrimitiveValue(std::string_view sv);
};
