#include "Parser.hpp"
#include "Log.hpp"
#include "Utils.hpp"
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <stack>

Parser::Parser(const std::string& file_name)
	:_file_name(file_name), _file() {
	std::error_code ec;
	/**
	 * Try to locate the file in the current file system, will throw an error
	 * if file does not exist
	 */
	if (!std::filesystem::exists(_file_name, ec) || ec) {
		throw ParserException(ERROR_LOG("File does not exist: " + file_name));
	}
	/**
	 * Will compare the file extension with the standard one and will throw
	 * an error in case of mismatch. Subsequent characters in the file_name after
	 * the last occurrence of '.'
	 */
	size_t pos = _file_name.rfind('.');
	std::string ext = _file_name.substr(pos + 1);
	if (ext != EXTENSION) {
		throw ParserException(ERROR_LOG("Wrong extension : " + _file_name));
	}

	_file.open(_file_name);

	/**
	 * if the file pointed by the file_name can not open, will throw an error
	 */
	if (_file.fail()) {
		throw ParserException(ERROR_LOG("Couldn't open file : " + _file_name + ": " + std::string(strerror(errno))));
	}

	/**
	 * If the file pointed by the file_name is empty, will throw an error
	 */
	if (std::filesystem::file_size(_file_name) == 0) {
		throw ParserException(ERROR_LOG("Empty file : " + _file_name));
	}
	/**
	 * Successfully opening the file and tokenizing the content
	 * all the tokens will be saved into AST tree structure
	 */
	tokenizeFile();
}

Parser::~Parser() {
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
void Parser::tokenizeFile(void) {
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
	for (const auto& node : root.children) {
		// check the if the node is a server block
		if (getKey(node) == "server") {
			// if node has a value
			if (node.children.size() > 1) {
				//extract first children
				const Token& content = node.children[1];

				if (!content.children.empty()) {

					for (const auto& block : content.children) {

						// first isolate all the ports related to a server config
						std::vector<std::string> collection = getCollectionBykey(block, "listen");

						// retrieve all the other data except ports
						Config config = convertToServerData(block);

						// add port one by one and create a copy of config
						if (collection.empty()) {
							throw ParserException(ERROR_LOG("Missing ports in config files!"));
						}

						for (auto& item : collection) {
							if (!isValidPort(item)) {
								_server_configs.clear();
								throw ParserException(ERROR_LOG("Invalid port value: " + item));
							} else {
								config.port = static_cast<uint16_t>(std::stoi(item));
								_server_configs.emplace_back(config);
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
const Config& Parser::getServerConfig(size_t index) {
	return (_server_configs.at(index));
}

/**
 * will return the whole configs vector for server construction.
 */
const std::vector<Config>	&Parser::getServerConfigs(void) const {
	return _server_configs;
}

/**
 * will return the size of the internal container, useful info when required
 * to loop through the entire vector
 */
size_t Parser::getNumberOfServerConfigs(void) {
	return (_server_configs.size());
}

/**
 * this function will convert a server data token to a server data struct
 *
 * @param server	block of data in the AST need to convert
 *
 * @return	value of the config created on the fly, will recreate the similar
 *			data in the respective vector, temporary data so no reference
 */
Config Parser::convertToServerData(const Token& block) {

	Config config;

	DEBUG_LOG("\tConverting server config tokens to server data");

	for (auto item : block.children) {
		if (item.children.size() < 2)
			continue;

		// extract the value of the key from the AST
		std::string	key = getKey(item);

		// set host or the IP address value
		if (key == "host") {
			std::string str = item.children.at(1).value;
			DEBUG_LOG("\t\tAdding host " + str);
			if (str != "localhost" && !isValidIPv4(str)) {
				throw ParserException(ERROR_LOG("Invalid IPv4 address value: " + str));
			}
			config.host = str;
		}

		if (key == "server_name") {
			DEBUG_LOG("\t\tAdding server_name " + item.children.at(1).value);
			config.serverName = item.children.at(1).value;
		}

		if (key == "directory_listing") {
			std::string	val = item.children.at(1).value;

			if (val != "true" && val != "false") {
				ERROR_LOG("Unrecognized value for directory_listing, retaining default value false");
			} else {
				config.directoryListing = (item.children.at(1).value == "true");
				DEBUG_LOG(std::string("\t\tSet directory listing to ") + (config.directoryListing ? "true" : "false"));
			}
		}

		if (key == "autoindex") {
			std::string	val = item.children.at(1).value;

			if (val != "true" && val != "false") {
				ERROR_LOG("Invalid value for autoindex: " + val);
			} else {
				config.autoindex = (item.children.at(1).value == "true");
				DEBUG_LOG(std::string("\t\tSet autoindexing to ") + (config.autoindex ? "true" : "false"));
			}
		}

		if (key == "status_pages") {
			for (auto e : item.children.at(1).children) {
				if (e.children.at(1).type != TokenType::Value) {
					ERROR_LOG("Invalid token type for status_pages, skipping");
					continue;
				}
				DEBUG_LOG("\t\tMapping status page "
					+ std::to_string(std::stoi(e.children.at(0).value))
					+ " to " + e.children.at(1).value);
				config.statusPages[e.children.at(0).value] = e.children.at(1).value;
			}
		}

		if (key == "client_max_body_size") {
			std::string	val = item.children.at(1).value;

			if (!std::all_of(val.begin(), val.end(), isdigit)) {
				ERROR_LOG("Invalid value for client_max_body_size: " + val);
				continue;
			}
			try {
				DEBUG_LOG("\t\tSetting requestMaxBodySize to " + val);
				config.requestMaxBodySize = std::stoul(val);
			} catch (std::exception const &e) {
				ERROR_LOG(std::string("Error setting requestMaxBodySize: ") + e.what());
			}
		}

		if (key == "routes") {
			for (auto r : item.children.at(1).children) {
				if (r.children.at(1).type != TokenType::Value)
					continue;
				DEBUG_LOG("\t\tAdding route " + r.children.at(0).value + " -> " + r.children.at(1).value);
				config.routes[r.children.at(0).value] = r.children.at(1).value;
			}
		}
	}
	return (config);
}

/**
 * this function extracts a collection of values from the AST and creates a vector of strings
 * @param root, key block of data in the AST need to convert
 * @return vector of strings
 */
std::vector<std::string> Parser::getCollectionBykey(const Token& root, const std::string& key) {
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
	return (collection);
}

/**
 * this function will check and return if a string is a valid JSON string in respect of
 * brackets, quotes, separators and primitive values
 */
bool Parser::isValidJSONString(std::string_view sv) {
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
				std::cerr << "Error: Invalid value format -> " << buffer << "\n";
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
					return (false);
				} else {
					brackets.pop();
				}
				break;
			case ']':
				if (brackets.empty() || brackets.top() != '[') {
					return (false);
				} else {
					brackets.pop();
				}
				break;
			case ':': // allowing to have contiguous ':' for IPv6 validation
				if (prevChar == ',') {
					return (false);
				}
				break;
			case ',':
				if (prevChar == ':' || prevChar == ',') {
					return (false);
				}
				break;
			default:
				buffer += c;
				break;
		}
		prevChar = c;
	}

	if (inQuotes) {
		std::cerr << "Un-closed double quotations !\n";
		return (false);
	}

	if (!brackets.empty()) {
		std::cerr << "Un-closed brackets " << brackets.top() << "!\n";
		return (false);
	}
	return (true);
}

/**
 * this function will check if a given string is a valid primitive value
 * an integer, a fractional value, IPv4, true or false
 */
bool Parser::isPrimitiveValue(std::string_view sv) {
	if (sv.empty())
		return (false);

	if (sv == "true" || sv == "false")
		return (true);

	if (isValidIPv4(sv) || isValidPort(sv) || isUnsignedIntLiteral(sv) || isPositiveDoubleLiteral(sv))
		return true;

	return false;
}
