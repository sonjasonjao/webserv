#pragma once

#include <fstream>
#include <cstdint>
#include <cctype>
#include <string>
#include <vector>
#include <stack>
#include <map>

#include "CustomException.hpp"
#include "JSON.hpp"
#include "Request.hpp"

/**
 * Default file_name extension for the configuration file, with out correct extension
 * file_name invalid.
 */
#define EXTENSION "json"

struct Config {
	std::string	host;		// IP or hostname on which this server listens, e.g. "0.0.0.0" or "127.0.0.1"
	std::string	serverName;	// List of server names (virtual hosts) handled by this server eg : {"example.com", "www.example.com"}

	uint16_t	port = 0;	// Port on which this server listens

	std::map<std::string, std::string>	status_pages;	// Mapping from HTTP status code to custom page path.
	std::map<std::string, std::string>	routes;			// Set of routes (URI -> path definitions) for this server.

	std::optional<size_t> client_max_body_size;			// Default maximum allowed size (in bytes) of the request body for this server
	std::optional<std::string> upload_dir;				// Default upload directory

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
	const Config&	getServerConfig(size_t index);

	const std::vector<Config>	&getServerConfigs(void) const;

	size_t	getNumberOfServerConfigs(void);

	Config	convertToServerData(const Token& server);

	std::vector<std::string>	getCollectionBykey(const Token& root, const std::string& key);

	bool	isValidJSONString(std::string_view sv);

	bool	isPrimitiveValue(std::string_view sv);
};
