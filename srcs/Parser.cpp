#include "Parser.hpp"
#include "Log.hpp"
#include "Utils.hpp"
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <stack>
#include <algorithm>

/**
 * The content of the configuration file will be tokenized and saved.
 *
 * @param file_name	Name of file to be parsed
 */
Parser::Parser(std::string const &fileName)
	:	_fileName(fileName), _file()
{

	if (!std::filesystem::exists(_fileName))
		throw ParserException(ERROR_LOG("File does not exist: " + _fileName));

	// File extension checking
	size_t		pos = _fileName.rfind('.');

	if (pos == std::string::npos || _fileName.begin() + pos + 1 == _fileName.end())
		throw ParserException(ERROR_LOG("Filename '" + _fileName + "' has no extension"));

	std::string	ext = _fileName.substr(pos + 1);

	if (ext != EXTENSION)
		throw ParserException(ERROR_LOG("Wrong extension: " + _fileName));

	_file.open(_fileName);
	if (_file.fail())
		throw ParserException(ERROR_LOG("Couldn't open file: " + _fileName + ": " + std::string(strerror(errno))));

	if (std::filesystem::file_size(_fileName) == 0)
		throw ParserException(ERROR_LOG("Empty file: " + _fileName));

	tokenizeFile();
}

Parser::~Parser()
{
	if (_file.is_open())
		_file.close();
}

/**
 * Will open the configuration file and read line by line, every line will be broken down
 * into tokens based on spaces/tabs. Tokens are saved to an internal container.
 */
void	Parser::tokenizeFile()
{
	std::string	line;
	std::string	output;

	while (getline(_file, line)) {
		line = trim(line);
		if (line.empty()) {
			continue;
		} else {
			output.append(line);
			line.clear();
		}
	}

	if (!isValidJsonString(output))
		throw ParserException(ERROR_LOG("Output not a valid JSON string"));

	// create Token AST for validation
	Token	root = createToken(output);

	/**
	 * Building vector of Config structs to hold all the configuration data.
	 * Configuration file should contain at least one server configuration.
	 * Anything other than "server" as the key will throw an error.
	*/
	for (auto const &node : root.children) {
		if (getKey(node) != "server")
			throw ParserException(ERROR_LOG("Bad key node: " + getKey(node)));
		if (node.children.size() < 2)
			throw ParserException(ERROR_LOG("Children size is less than 2"));

		Token const	&content = node.children[1];

		if (content.children.empty())
			throw ParserException(ERROR_LOG("Content node for key '" + getKey(node) +"' has no children"));

		for (auto const &block : content.children) {

			// Isolate all the ports related to a server config
			std::vector<std::string>	collection = getCollectionBykey(block, "listen");

			// Retrieve all the other data except ports
			Config	config = convertToServerData(block);

			// Add port one by one and create a copy of config
			if (collection.empty())
				throw ParserException(ERROR_LOG("Missing ports in config file"));

			for (auto &item : collection) {
				if (!isValidPort(item)) {
					_serverConfigs.clear();
					throw ParserException(ERROR_LOG("Invalid port value: " + item));
				}
				config.port = static_cast<uint16_t>(std::stoi(item));
				_serverConfigs.emplace_back(config);
			}
		}
	}
}

/**
 * @exception	Invalid indices will trigger out of bound exceptions
 *
 * @param	Server index
 *
 * @return	Constant reference to configuration struct matching the index
 */
Config const	&Parser::getServerConfig(size_t index) const
{
	return _serverConfigs.at(index);
}

/**
 * @return	Constant reference to server configurations struct
 */
std::vector<Config> const	&Parser::getServerConfigs() const
{
	return _serverConfigs;
}

/**
 * @return	Size of the server configurations struct
 */
size_t	Parser::getNumberOfServerConfigs()
{
	return _serverConfigs.size();
}

/**
 * @param block	Part of the node tree to be converted
 *
 * @return	Config with parsed information from the token node tree
 */
Config	Parser::convertToServerData(Token const &block)
{
	Config config;

	DEBUG_LOG("Converting server config tokens to server data");

	for (auto const &item : block.children) {
		if (item.children.size() != 2)
			throw ParserException(ERROR_LOG("\tbad key value pair"));

		std::string	key = getKey(item);
		Token		tok = item.children.at(1);

		if (key.empty())
			throw ParserException(ERROR_LOG("\tEmpty key"));

		std::vector<std::string> const	pureStringFields {
			"server_name",
			"upload_dir"
		};

		std::vector<std::string> const	stringOrPrimitiveFields {
			"host",
			"directory_listing",
			"autoindex",
			"client_max_body_size"
		};

		/* ---- String fields ---- */
		if (std::find(pureStringFields.begin(), pureStringFields.end(), key) != pureStringFields.end()) {

			if (tok.type != TokenType::Value)
				throw ParserException(ERROR_LOG("\tInvalid token type for '" + key + "'"));
			if (tok.value.empty())
				throw ParserException(ERROR_LOG("\tEmpty token value for '" + key + "'"));

			DEBUG_LOG("\t" + key + " = " + tok.value);

			if (key == "server_name")
				config.serverName = tok.value;
			else if (key == "upload_dir")
				config.uploadDir = tok.value;

			continue;
		} /* ---- String or primitive fields ---- */
		else if (std::find(stringOrPrimitiveFields.begin(), stringOrPrimitiveFields.end(), key) != stringOrPrimitiveFields.end()) {

			if (tok.type != TokenType::Value && tok.type != TokenType::Primitive)
				throw ParserException(ERROR_LOG("\tInvalid token type for '" + key + "'"));
			if (tok.value.empty())
				throw ParserException(ERROR_LOG("\tEmpty token value for '" + key + "'"));

			// Set host or the IP address value
			if (key == "host") {
				if (tok.value != "localhost" && !isValidIPv4(tok.value))
					throw ParserException(ERROR_LOG("\tInvalid IPv4 address value: " + tok.value));

				DEBUG_LOG("\thost = " + tok.value);
				config.host = tok.value;

				continue;
			}

			if (key == "directory_listing" || key == "autoindex") {
				if (tok.value != "true" && tok.value != "false")
					throw ParserException(ERROR_LOG("\tInvalid value for '" + key + "': " + tok.value));

				DEBUG_LOG("\t" + key + " = " + tok.value);

				if (key == "directory_listing")
					config.directoryListing = (tok.value == "true");
				else
					config.autoindex = (tok.value == "true");

				continue;
			}

			if (key == "client_max_body_size") {
				if (!std::all_of(tok.value.begin(), tok.value.end(), isdigit) || !isUnsignedIntLiteral(tok.value))
					throw ParserException(ERROR_LOG("\tInvalid value for '" + key + "': " + tok.value));

				try {
					config.clientMaxBodySize = std::stoul(tok.value);
					DEBUG_LOG("\t" + key + " = " + tok.value);

					continue;
				} catch (std::exception const &e) {
					throw ParserException(ERROR_LOG(std::string("\tError setting '" + key + "': ") + e.what()));
				}
			}
		} /* ---- Status pages ---- */
		else if (key == "status_pages") {
			DEBUG_LOG("\tSetting status pages");
			if (tok.type != TokenType::Object)
				throw ParserException(ERROR_LOG("\tInvalid token type for '" + key + "'"));

			for (auto const &e : tok.children) {
				if (e.children.size() != 2)
					throw ParserException(ERROR_LOG("\t\t'" + key + "': bad key value pair"));

				if (e.children.at(1).type != TokenType::Value)
					throw ParserException(ERROR_LOG("\t\tInvalid token type for '" + key + "' target"));

				std::string	pageNumber	= e.children.at(0).value;
				std::string	route		= e.children.at(1).value;

				if (pageNumber.length() != 3 || !std::all_of(pageNumber.begin(), pageNumber.end(), isdigit))
					throw ParserException(ERROR_LOG("\t\tInvalid key '" + pageNumber + "' for '" + key + "'"));

				DEBUG_LOG("\t\t" + pageNumber + " -> " + route);
				config.statusPages[pageNumber] = route;

				continue;
			}
		} /* ---- Routes ---- */
		else if (key == "routes") {
			DEBUG_LOG("\tSetting routes");
			if (tok.type != TokenType::Object)
				throw ParserException(ERROR_LOG("\tInvalid token type for '" + key + "'"));

			for (auto const &r : tok.children) {
				if (r.children.size() != 2) {
					ERROR_LOG("\t\t" + key + ": bad key value pair");
					continue;
				}

				Token	const &leftToken	= r.children.at(0);
				Token	const &rightToken	= r.children.at(1);

				if (rightToken.type != TokenType::Object && rightToken.type != TokenType::Value)
					throw ParserException(ERROR_LOG("\t\tInvalid token type for '" + key + "' target"));

				std::string	uri		= leftToken.value;
				Route		route;

				// Simplest case, simple key value, allowed methods unspecified
				if (rightToken.type == TokenType::Value) {
					DEBUG_LOG("\t\t" + uri + " -> " + rightToken.value);
					route.original		= uri;
					route.target		= rightToken.value;
					config.routes[uri]	= route;

					continue;
				}

				// Object case
				if (rightToken.children.size() != 2)
					throw ParserException(ERROR_LOG("\t\t" + key + ": wrong number of key value pairs"));

				for (auto const &e : rightToken.children) {
					if (e.children.size() != 2)
						throw ParserException(ERROR_LOG("\t\t\t" + key + ": wrong number of key value pairs"));

					Token const	&left	= e.children.at(0);
					Token const	&right	= e.children.at(1);

					// Order of target and allowed methods is free to choose
					if (left.value == "target" && right.type == TokenType::Value) {
						route.original = uri;
						route.target = right.value;
					} else if (left.value == "allowed_methods") {
						if (right.type != TokenType::Array)
							throw ParserException(ERROR_LOG("\t\t\tIncorrect token type for '" + left.value + "' target"));

						route.allowedMethods = std::vector<std::string>();
						for (auto const &a : right.children) {
							if (a.value != "GET" && a.value != "POST" && a.value != "DELETE")
								throw ParserException(ERROR_LOG("\t\t\t\tIncorrect value '" + a.value + "' for '" + left.value + "' array element"));

							route.allowedMethods->emplace_back(a.value);
						}
					}
				} // Target or allowed methods missing -> error
				if (route.target.empty() || !route.allowedMethods.has_value())
					throw ParserException(ERROR_LOG("\t\t\tBad route configuration"));

				DEBUG_LOG("\t\t" + uri + " -> " + route.target);
				DEBUG_LOG("\t\tAllowed methods");
				#if DEBUG_LOGGING
				for (auto const &a : route.allowedMethods.value())
					DEBUG_LOG("\t\t\t" + a);
				#endif

				config.routes[uri] = route;

				continue;
			}
		} /* ---- Allowed methods server level ---- */
		else if (key == "allowed_methods") {
			if (tok.type != TokenType::Array)
				throw ParserException(ERROR_LOG("\tInvalid token type for '" + key + "'"));

			config.allowedMethods = std::vector<std::string>();
			for (auto const &a : tok.children) {
				if (a.value != "GET" && a.value != "POST" && a.value != "DELETE")
					throw ParserException(ERROR_LOG("\t\tIncorrect value '" + a.value + "' for allowed_methods array element"));
				if (std::find(config.allowedMethods->begin(), config.allowedMethods->end(), a.value) != config.allowedMethods->end())
					throw ParserException(ERROR_LOG("\t\tDuplicate '" + a.value + "' for allowed_methods array element"));

				config.allowedMethods->emplace_back(a.value);
			}
			DEBUG_LOG("\tAllowed methods");
			#if DEBUG_LOGGING
			for (auto const &a : config.allowedMethods.value())
				DEBUG_LOG("\t\t" + a);
			#endif

			continue;
		} /* ---- Everything else ---- */
		else {
			if (key != "listen")
				throw ParserException(ERROR_LOG("\tUnknown option: " + key));
		}
	}
	return config;
}

/**
 * @param root	Block of data in the AST to match the key against
 * @param key	Key string for field matching
 *
 * @return	Vector of extracted values from nodes that match the key
 */
std::vector<std::string>	Parser::getCollectionBykey(Token const &root, std::string const &key)
{
	std::vector<std::string>	collection;

	for (auto item : root.children) {
		std::string	itemKey = getKey(item);

		if (key == itemKey && item.children.size() > 1) {
			for (auto p : item.children.at(1).children)
				collection.emplace_back(p.value);
		}
	}
	return collection;
}

/**
 * @param sv	String view of possibly valid or invalid JSON
 *
 * @return	true if string view contains valid JSON, false if not
 */
bool	Parser::isValidJsonString(std::string_view sv)
{
	std::stack<char>	brackets;
	std::string			buffer;
	bool				inQuotes = false;
	char				prevChar = '\0';

	for (size_t i = 0; i < sv.size(); ++i) {
		char	c = sv[i];

		// Double quotes without leading escape character toggles inQuotes
		if (c == '"' && prevChar != '\\') {
			inQuotes = !inQuotes;
			prevChar = c;
			continue;
		}

		// If already inside the quotes all character are allowed, update previous char and continue
		if (inQuotes) {
			prevChar = c;
			continue;
		}

		bool	isSeparator = (std::isspace(c) || c == ':' || c == ',' || c == '}' || c == ']');

		// Check for valid values that are not expected to be surrounded by quotes
		if (isSeparator && !buffer.empty()) {
			if (!isPrimitiveValue(buffer)) {
				ERROR_LOG("Invalid value format -> " + buffer + "\n");
				return false;
			}
			buffer.clear();
		}

		// Skip all whitespace
		if (std::isspace(c)) {
			prevChar = c;
			continue;
		}

		switch (c) {
			case '{':	brackets.push(c);	break;
			case '[':	brackets.push(c);	break;
			case '}':
				if (brackets.empty() || brackets.top() != '{')
					return false;
				else
					brackets.pop();
			break;
			case ']':
				if (brackets.empty() || brackets.top() != '[')
					return false;
				else
					brackets.pop();
			break;
			case ':': // Allowing to have contiguous ':' for IPv6 validation  NOTE: We don't use IPv6 anymore I think?
				if (prevChar == ',')
					return false;
			break;
			case ',':
				if (prevChar == ':' || prevChar == ',')
					return false;
			break;
			default:
				buffer += c;
			break;
		}
		prevChar = c;
	}

	if (inQuotes) {
		ERROR_LOG("Un-closed double quotations !\n");
		return false;
	}

	if (!brackets.empty()) {
		ERROR_LOG(std::string("Un-closed brackets ") + brackets.top() + "!\n");
		return false;
	}
	return true;
}

/**
 * @param sv	String view to be checked
 *
 * @return	true if string view contains a primitive value, false if not
 */
bool	Parser::isPrimitiveValue(std::string_view sv)
{
	if (sv.empty())
		return false;

	if (sv == "true" || sv == "false")
		return true;

	if (isValidIPv4(sv) || isValidPort(sv) || isUnsignedIntLiteral(sv) || isPositiveDoubleLiteral(sv))
		return true;

	return false;
}
