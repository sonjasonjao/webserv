#include "Parser.hpp"
#include "Log.hpp"
#include "Utils.hpp"
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <stack>
#include <algorithm>

Parser::Parser(std::string const &fileName)
	:	_fileName(fileName), _file()
{
	std::error_code ec;
	/**
	 * Try to locate the file in the current file system, will throw an error
	 * if file does not exist
	 */
	if (!std::filesystem::exists(_fileName, ec) || ec) {
		throw ParserException(ERROR_LOG("File does not exist: " + _fileName));
	}
	/**
	 * Will compare the file extension with the standard one and will throw
	 * an error in case of mismatch. Subsequent characters in the file_name after
	 * the last occurrence of '.'
	 */
	size_t pos = _fileName.rfind('.');
	std::string ext = _fileName.substr(pos + 1);
	if (ext != EXTENSION) {
		throw ParserException(ERROR_LOG("Wrong extension : " + _fileName));
	}

	_file.open(_fileName);

	/**
	 * if the file pointed by the file_name can not open, will throw an error
	 */
	if (_file.fail()) {
		throw ParserException(ERROR_LOG("Couldn't open file : " + _fileName + ": " + std::string(strerror(errno))));
	}

	/**
	 * If the file pointed by the file_name is empty, will throw an error
	 */
	if (std::filesystem::file_size(_fileName) == 0) {
		throw ParserException(ERROR_LOG("Empty file : " + _fileName));
	}
	/**
	 * Successfully opening the file and tokenizing the content
	 * all the tokens will be saved into AST tree structure
	 */
	tokenizeFile();
}

Parser::~Parser()
{
	/**
	 * At the end if the file descriptor is still open, it will be closed gracefully
	 */
	if (_file.is_open()) {
		_file.close();
	}
}

/**
 * Will open the configuration file and read line by line, every line will be break down
 * to tokens based on spaces/tabs
 * @param void - class method will have access to all the class attributes
 * @return void - all the tokens will be saved to an internal container
 */
void Parser::tokenizeFile()
{
	std::string	line;
	std::string	output;

	// read a line
	while (getline(_file, line)) {
		// remove leading/trailing white spaces
		line = trim(line);
		// if line is empty, skip
		if (line.empty()) {
			continue;
		} else {
			// concatinate output and reset line for next read
			output.append(line);
			line.clear();
		}
	}

	// JSON string validation
	if (!isValidJSONString(output)) {
		throw ParserException(ERROR_LOG("Output not a valid JSON string"));
	}

	// create Token AST for validation
	Token root = createToken(output);

	/**
	 * building configuration struct vector to hold all the configuration data
	 * configuration file should contain at least one server configuration
	 * anything other than "server" as the key will throw an error
	*/
	for (auto const &node : root.children) {
		// check the if the node is a server block
		if (getKey(node) == "server") {
			// if node has a value
			if (node.children.size() > 1) {
				//extract first children
				Token const	&content = node.children[1];

				if (!content.children.empty()) {
					for (auto const &block : content.children) {

						// first isolate all the ports related to a server config
						std::vector<std::string> collection = getCollectionBykey(block, "listen");

						// retrieve all the other data except ports
						Config config = convertToServerData(block);

						// add port one by one and create a copy of config
						if (collection.empty()) {
							throw ParserException(ERROR_LOG("Missing ports in config files!"));
						}

						for (auto &item : collection) {
							if (!isValidPort(item)) {
								_serverConfigs.clear();
								throw ParserException(ERROR_LOG("Invalid port value: " + item));
							} else {
								config.port = static_cast<uint16_t>(std::stoi(item));
								_serverConfigs.emplace_back(config);
							}
						}
					}
				} else {
					// server key exists but has no content: treat as malformed configuration
					throw ParserException(ERROR_LOG("Incorrect configuration!"));
				}
			} else {
				// server key exists but is missing expected children: treat as malformed configuration
				throw ParserException(ERROR_LOG("Incorrect configuration!"));
			}
		} else {
			throw ParserException(ERROR_LOG("Incorrect configuration!"));
		}
	}
}

/**
 * this will return the server configuration info to any valid request,
 * pointed by index
 * @exception will throw an out of bound exception in case request invalid index
 * caller has to handle the exception
 * @param index unsigned int value which represents the index of the server
 * config struct data
 * @return const reference to the requested data structure
 */
const Config	&Parser::getServerConfig(size_t index)
{
	return _serverConfigs.at(index);
}

/**
 * will return the whole configs vector for server construction.
 */
const std::vector<Config>	&Parser::getServerConfigs() const
{
	return _serverConfigs;
}

/**
 * will return the size of the internal container, useful info when required
 * to loop through the entire vector
 */
size_t	Parser::getNumberOfServerConfigs()
{
	return _serverConfigs.size();
}

/**
 * this function will convert a server data token to a server data struct
 *
 * @param server	block of data in the AST need to convert
 *
 * @return	value of the config created on the fly, will recreate the similar
 *			data in the respective vector, temporary data so no reference
 */
Config Parser::convertToServerData(Token const &block)
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

			// set host or the IP address value
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
 * this function extracts a collection of values from the AST and creates a vector of strings
 * @param root, key block of data in the AST need to convert
 * @return vector of strings
 */
std::vector<std::string>	Parser::getCollectionBykey(Token const &root, std::string const &key)
{
	std::vector<std::string> collection;
	for (auto item : root.children) {
		std::string itemKey = getKey(item);
		if (key == itemKey) {
			if (item.children.size() > 1) {
				for (auto p : item.children.at(1).children) {
					collection.emplace_back(p.value);
				}
			}
		}
	}
	return collection;
}

/**
 * this function will check and return if a string is a valid JSON string in respect of
 * brackets, quotes, separators and primitive values
 */
bool Parser::isValidJSONString(std::string_view sv)
{
	std::stack<char> brackets;
	std::string buffer;

	bool inQuotes = false;
	char prevChar = '\0';

	for (size_t i = 0; i < sv.size(); ++i) {
		char c = sv[i];

		/**
		 * if there is double quotes with out escape character, then will toggle
		 * inQuotes
		 */
		if (c == '"' && prevChar != '\\') {
			inQuotes = !inQuotes;
			prevChar = c;
			continue;
		}

		/**
		 * if already inside the quotes any character is allowed,
		 * simply update the previous char and continue
		 */
		if (inQuotes) {
			prevChar = c;
			continue;
		}

		/**
		 * isolating separators
		 */
		bool isSeparator = (std::isspace(c) || c == ':' || c == ',' || c == '}' || c == ']');

		/**
		 * check for valid values that are not expected to surrounded by quotes
		 */
		if (isSeparator && !buffer.empty()) {
			if (!isPrimitiveValue(buffer)) {
				ERROR_LOG("Invalid value format -> " + buffer + "\n");
				return false;
			}
			buffer.clear();
		}

		/**
		 * all the isspace characters even outside the double quotes will skip
		 */
		if (std::isspace(c)) {
			prevChar = c;
			continue;
		}

		switch (c)
		{
			case '{':
				brackets.push(c);
				break;
			case '[':
				brackets.push(c);
				break;
			case '}':
				if (brackets.empty() || brackets.top() != '{') {
					return false;
				} else {
					brackets.pop();
				}
				break;
			case ']':
				if (brackets.empty() || brackets.top() != '[') {
					return false;
				} else {
					brackets.pop();
				}
				break;
			case ':': // allowing to have contiguous ':' for IPv6 validation
				if (prevChar == ',') {
					return false;
				}
				break;
			case ',':
				if (prevChar == ':' || prevChar == ',') {
					return false;
				}
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
 * this function will check if a given string is a valid primitive value
 * an integer, a fractional value, IPv4, true or false
 */
bool Parser::isPrimitiveValue(std::string_view sv)
{
	if (sv.empty())
		return false;

	if (sv == "true" || sv == "false")
		return true;

	if (isValidIPv4(sv) || isValidPort(sv) || isUnsignedIntLiteral(sv) || isPositiveDoubleLiteral(sv))
		return true;

	return false;
}
