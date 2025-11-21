#include "Parser.hpp"

Parser::Parser(std::string& file_name)
    :_file_name(file_name), _file() {
        std::error_code ec;
        /**
         * Try to locate the file in the current file system, will throw an error
         * if file is not exsists
        */
        if(!std::filesystem::exists(_file_name, ec) || ec) {
            throw ParserException(
                "Error : configuration file not exist <" + _file_name + ">"
            );
        }
        /**
         * Will compare the file extesnsion with the standard one and will throw
         * an error in case mismatch. Subsequent characters in the file_name after
         * the last occurence of '.'
        */
        size_t pos = _file_name.rfind('.');
        std::string ext = _file_name.substr(pos + 1);
        if(ext != EXTENSION) {
            throw ParserException(
                "Error : wrong configuration file extension <" + _file_name + ">"
            );
        }

        _file.open(_file_name);

        /**
         * if the file pointed by the file_name can not open, will throw an error
        */
        if(_file.fail()) {
            throw ParserException(
                "Error : can not open configuration file <" + _file_name + ">"
            );
        }

        /**
         * If the file pointed by the file_name is empty, will throw and error
         */
        if(std::filesystem::file_size(_file_name) == 0) {
            throw ParserException(
                "Error : empty configutaion file <" + _file_name + ">"
            );
        }
    /**
     * Sucessfully opening the file and tokenizing the content
    */
    tokenize_file();
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
void Parser::tokenize_file(void) {
    std::string line;

    while(getline(_file, line)) {
        std::stringstream ss(line);
        std::string word;
        while(ss >> word) {
            _tokens.push_back(word);
            word.clear();
        }
        line.clear();
    }
}

void Parser::print_tokens(void) {
    for(auto it : _tokens){
        std::cout << it << "\n";
    }
}
