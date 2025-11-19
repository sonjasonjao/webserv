#include "Parser.hpp"

Parser::Parser():str("Error"){}

Parser::Parser(std::string& phrase):str(phrase){}

Parser::~Parser(){}

std::string Parser::getMessage(void) {
    if(str == "Error"){
        throw ParserException("Default error!");
    }
    return (this->str);
}
