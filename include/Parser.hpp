#pragma once

#include <string>

#include "CustomeException.hpp"

class Parser {
    private:
        std::string str;

    public:
        Parser();
        Parser(std::string& phrase);
        Parser(const Parser& other) = delete;
        Parser& operator=(const Parser& other) = delete;
        ~Parser();
        struct ParserException : public CustomeException {
            ParserException(const std::string& str):
                CustomeException(str){}
        };

        std::string getMessage(void);
};
