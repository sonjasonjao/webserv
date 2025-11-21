#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <filesystem>
#include <iostream>

#include "CustomeException.hpp"

/**
 * Default file_name extension for the configuration file, with out correct extension
 * file_name invalid.
*/
#define EXTENSION "config"

class Parser {
    private:
        std::string const           _file_name;
        std::ifstream               _file;
        std::vector<std::string>    _tokens;

    public:
        /**
         * proper-way of creating Parser instance
         * @param std::string file_name - file name need to parse
         * @return void - content of the file will be tokenize and save to a internal
         * container, type std::vector
        */
        Parser(std::string& file_name);
        /**
         * only way of creating Parser instanse should be argument constructor
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
        struct ParserException : public CustomeException {
            ParserException(const std::string& str):
                CustomeException(str){}
        };

        /**
         * Correct configuration file will be read line by line, every line will be tokenize
         * and svaed to the _tokens
         * @param void
         * @return void
        */
        void tokenize_file(void);

       /**
        * Debugging helper funtion, will remove later
       */
        void print_tokens(void);
};
