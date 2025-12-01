#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <algorithm>
#include <vector>

/**
 * Pascal case to follow the modern c++ standards
*/
enum class TokenType {
    Object,
    Array,
    Identifier,
    Element,
    Value,
    Null
};

struct Token {
    TokenType           type;
    std::string         value;
    std::vector<Token>  children;
};

void printToken(const Token& root, int indent = 0);

std::string typeToString(TokenType type);
std::string trim(std::string_view sv);
std::string getKey(const Token& token);
std::string removeQuotes(const std::string& str);

std::vector<std::string> splitElements(std::string_view sv);

size_t unquotedDelimiter(std::string_view sv, const char c);

TokenType getTokenType(const std::string& str);

Token createToken(const std::string& str);
Token createToken(const std::string& str, TokenType type);
