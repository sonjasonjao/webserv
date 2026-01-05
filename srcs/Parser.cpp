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
                        Config config = convertToServerData(block);
                        _server_configs.emplace_back(config);
                    }
                }
            }
        } else {
            throw std::runtime_error("Incorrect confirguartion !");
        }
    }
    // for(const auto& node : root.children) {
    //     std::cout << getKey(node) << "\n";
    //     if(node.children.size() > 1){
    //         for(const auto& it : node.children){
    //             printToken(it, 1);
    //         }
    //     }
    // }
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
    std::vector<std::string> collection = getCollectionBykey(block, "listen");

    for(auto it : collection) {
        std::cout << it << "\n";
    }
    std::cout << "-------------------------------\n";
    return (config);
}

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