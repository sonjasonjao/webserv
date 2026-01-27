#pragma once


#include "CustomException.hpp"
#include "JSON.hpp"
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>
#include <cctype>

/**
 * Default file_name extension for the configuration file, with out correct extension
 * file_name invalid.
 */
#define EXTENSION "json"

struct Route {
	std::string								original;
	std::string								target;
	std::optional<std::vector<std::string>>	allowedMethods;
};

struct Config {
	std::string	host;		// IP on which this server listens, e.g. "0.0.0.0", "127.0.0.1" or "localhost"
	std::string	serverName;	// Assigned name of server in config file, e.g. "localhost" or "www.example.com"

	std::optional<std::string>	uploadDir;	// Activates or deactivates upload directory behavior

	uint16_t	port = 0;	// Port on which this server listens

	std::map<std::string, std::string>	statusPages;	// Mapping from HTTP status code to custom page path.
	std::map<std::string, Route>		routes;			// Set of routes (URI -> path definitions) for this server.

	std::optional<std::vector<std::string>>	allowedMethods;	// Server level allowed HTTP request methods.

	std::optional<size_t>		clientMaxBodySize;	// Default maximum allowed size (in bytes) of the request body for this server.

	bool	directoryListing	= false;
	bool	autoindex			= false;
};

class Parser {
private:
	std::string const	_fileName;			// File name of the configuration file.
	std::ifstream		_file;				// ifstream instance to read the configuration file.
	std::vector<Config>	_serverConfigs;		// List of fully parsed server configurations built from the token list.

public:
	/**
	 * proper-way of creating Parser instance
	 * @param std::string file_name - file name need to parse
	 * @return void - content of the file will be tokenize and save to a internal
	 * container, type std::vector
	 */
	Parser(const std::string& fileName);	// The only way of creating a Parser instance should be via the argument constructor.
	Parser() = delete;
	Parser(const Parser& other) = delete;
	Parser& operator=(const Parser& other) = delete;
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
	 */
	void tokenizeFile();

	const Config&				getServerConfig(size_t index);
	const std::vector<Config>	&getServerConfigs() const;
	std::vector<std::string>	getCollectionBykey(const Token& root, const std::string& key);
	size_t						getNumberOfServerConfigs();

	Config	convertToServerData(const Token& server);

	bool	isValidJSONString(std::string_view sv);
	bool	isPrimitiveValue(std::string_view sv);
};
