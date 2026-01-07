#include "Parser.hpp"

Parser::Parser(const std::string& file_name)
    :_file_name(file_name), _file() {
        std::error_code ec;
        /**
         * Try to locate the file in the current file system, will throw an error
         * if file is not exsists
        */
        if(!std::filesystem::exists(_file_name, ec) || ec) {
            throw ParserException("File not exist : " + file_name);
        }
        /**
         * Will compare the file extesnsion with the standard one and will throw
         * an error in case mismatch. Subsequent characters in the file_name after
         * the last occurence of '.'
        */
        size_t pos = _file_name.rfind('.');
        std::string ext = _file_name.substr(pos + 1);
        if(ext != EXTENSION) {
            throw ParserException("Wrong extension : " + _file_name);
        }

        _file.open(_file_name);

        /**
         * if the file pointed by the file_name can not open, will throw an error
        */
        if(_file.fail()) {
            throw ParserException("Can not open file : " + _file_name);
        }

        /**
         * If the file pointed by the file_name is empty, will throw and error
         */
        if(std::filesystem::file_size(_file_name) == 0) {
            throw ParserException("Empty file : " + _file_name);
        }
    /**
     * Sucessfully opening the file and tokenizing the content
     * all the toiken will be sacved into AST tree structure
    */
    try {    
        tokenizeFile();
    }
    catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
}

Parser::~Parser() {
    /**
     * At the end if the file descriptor is still open, it will  be closed gracefully
    */
    if(_file.is_open()) {
        _file.close();
    }
}

/**
 * Will open the configuration file and read line by line, every line will be break down
 * to tokens base on spaces/tabs
 * @param void - class method will have access to all the class attributes
 * @return void - all the token will save to an internal container
*/
void Parser::tokenizeFile(void) {
    std::string line;
    std::string output;
    // read a line
    while(getline(_file, line)) {
        // remove leading/trailing white spaces
        line = trim(line);
        // if line is empty, skip
        if(line.empty()) {
            continue;
        } else {
            // concatinate output and reset line for next read
            output.append(line);
            line.clear();
        }
    }

    // JSON string validation
    if(!isValidJSONString(output)) {
        throw std::runtime_error("Incorrect confirguartion !");
    }

    // create Token AST for validation
    Token root = createToken(output);
    // buliding configuration struct vector to holds all the configuration data
    // configuration file hould conatins at least one srever configuration
    // anything other than "server" as the key will throw an error 
    for(const auto& node : root.children) {
        if(getKey(node) == "server") {
            if(node.children.size() > 1) {
                const Token& content = node.children[1];
                if(!content.children.empty()) {
                    for(const auto& block : content.children) {
                        // first isolate al the ports related to a server config
                        std::vector<std::string> collection = getCollectionBykey(block, "listen");
                        // retrive all the other data except ports
                        Config config = convertToServerData(block);
                        // add port one by one and create a copy of config
                        if(!collection.empty()) {
                            for(auto item : collection) {
                                auto port = std::stoi(item);
                                config.port = port;
                                _server_configs.emplace_back(config);
                            }
                        }
                    }
                }
            }
        } else {
            throw std::runtime_error("Incorrect confirguartion !");
        }
    }
}

/**
 * this will return the srever configuration info to any valid request,
 * pointed by index
 * @exception will throw and out of bound exception in case reqeust invalid index
 * caller has to handle the exception
 * @param index unisigned int value which represent the index of the server
 * config struct data
 * @return const reference to the requested data structure
*/
const Config& Parser::getServerConfig(size_t index) {
    return (_server_configs.at(index));
}

/**
 * will return the size of the inetrnal container, usefull info when required
 * to loop through the entire vector
*/
size_t Parser::getNumberOfServerConfigs(void) {
    return (_server_configs.size());
}

/**
 * this fucntion will conver a srever data token to a srever data struct
 * @param server block of data in the AST need to convert
 * @return value of the config create on the fly, will recreate the similar
 * data int the respective vector, temporary data so no reference
*/
Config Parser::convertToServerData(const Token& block) {
    Config config;
    for(auto item : block.children) {
        std::string key = getKey(item);
        if(key == "host") {
            if(item.children.size() > 1) {
                config.host = item.children.at(1).value;
            }
        } else if (key == "host_name") {
            if(item.children.size() > 1) {
                config.host_name = item.children.at(1).value;
            }
        }
    }
    return (config);
}

/**
 * this fucntion extract collection of values from the AST and created a vector of strings
 * @param root, key block of data in the AST need to convert
 * @return vector of strings 
*/
std::vector<std::string> Parser::getCollectionBykey(const Token& root, const std::string& key) {
    std::vector<std::string> collection;
    for(auto item : root.children) {
        std::string key_value = getKey(item);
        if (key == key_value) {
            if(item.children.size() > 1) {
                for(auto p : item.children.at(1).children) {
                    collection.emplace_back(p.value);
                }
            }
        }
    }
    return (collection);
}

/**
 * this function will check and return if a string is a valid JSON string in respcet of
 * brackets, quotes, sperators and primitive values
 */
bool Parser::isValidJSONString(std::string_view sv) {
    std::stack<char> brackets;
    std::string buffer;

    bool inQuotes = false;
    char prevChar = '\0';
    
    for(size_t i = 0; i < sv.size(); ++i) {
        char c = sv[i];

        /**
         * if there is double quotes with out escape charater, then will toggle
         * inQuotes
        */
        if(c == '"' && prevChar != '\\') {
            inQuotes = !inQuotes;
            prevChar = c;
            continue;
        }

        /**
         * if already inside the quotes any character is allowed, 
         * simply update the previous char and continue
        */
        if(inQuotes) {
            prevChar = c;
            continue;
        }

        /**
         * isolating sperators
        */
        bool isSperator = (std::isspace(c) || c == ':' || c == ',' || c == '}' || c == ']');

        /**
         * check for valid values that are not expected to surrounded by quotes
        */
        if(isSperator && !buffer.empty()) {
            if(!isPrimitiveValue(buffer)) {
                std::cerr << "Error: Invalid value format -> " << buffer << "\n";
                return false;
            }
            buffer.clear();
        }
        
        /**
         * all the isspace characters even outside the double quotes will skip 
        */
        if(std::isspace(c)) {
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
                if( brackets.empty() || brackets.top() != '{') {
                    return (false);
                } else {
                    brackets.pop();
                }
                break;
            case ']':
                if( brackets.empty() || brackets.top() != '[') {
                    return (false);
                } else {
                    brackets.pop();
                }
                break;
            case ':': // allowing to have contiguous ':' for IPv6 validation 
                if(prevChar == ',') {
                    return (false);
                }
                break;
            case ',':
                if(prevChar == ':' || prevChar == ',') {
                    return (false);
                }
                break;
            default:
                buffer += c;
                break;
        }
        prevChar = c;
    }

    if(inQuotes) {
        std::cerr << "Un-closed double quotations !\n";
        return (false);
    }

    if(!brackets.empty()) {
        std::cerr << "Un-closed brackets " << brackets.top() << "!\n";
        return (false);
    }
    return (true);
  
}

/**
 * this fucntion will check if a given string is a valid primitive value
 * an integer, a fractional value, IPv4 or IPv6 address, true or false
*/
bool Parser::isPrimitiveValue(std::string_view sv) {
     if(sv.empty()) {
        return (true);
    }

    if(sv == "true" || sv == "false") {
        return (true);
    }

    for(size_t i = 0; i < sv.size(); ++i) {
        char c = sv[i];
        if ((c != '.' || c != '+' || c != '-' || !std::isdigit(c))) {
            return (false);
        }
    }

    return (true);
}