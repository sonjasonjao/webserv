#pragma once

#include <string>
#include <string_view>
#include <vector>

enum class TokenType {
	Object,
	Array,
	Identifier,
	Element,
	Value,
	Primitive,
	Null
};

struct Token {
	TokenType			type;
	std::string			value;
	std::vector<Token>	children;
};

void	printToken(Token const &root, int indent = 0);

std::string	typeToString(TokenType type);
std::string	trim(std::string_view sv);
std::string	getKey(Token const &token);
std::string	removeQuotes(std::string const &str);

std::vector<std::string>	splitElements(std::string_view sv);

size_t	unquotedDelimiter(std::string_view sv, char const c);

TokenType	getTokenType(std::string const &str);

Token	createToken(std::string const &str);
Token	createToken(std::string const &str, TokenType type);
