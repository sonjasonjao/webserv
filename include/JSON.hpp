#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <algorithm>
#include <vector>

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

void print_token(const Token& root, int indent = 0);

std::string type_to_string(TokenType type);
std::string trim(std::string_view sv);
std::vector<std::string> split_elements(const std::string& str);

size_t unquoted_delimiter(std::string_view sv, const char c);

TokenType get_token_type(const std::string& str);

Token create_token(const std::string& str);
Token create_token(const std::string& str, TokenType type);
