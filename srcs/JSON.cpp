#include "JSON.hpp"

/**
 * this function will print the AST recursively, pure debugging function
 */
void printToken(const Token& root, int indent)
{
	for (int i = 0; i < indent; ++i) {
		std::cout << " ";
	}
	std::cout << typeToString(root.type);
	if (!root.value.empty()) {
		std::cout << "(" << root.value << ")";
	}
	std::cout << "\n";
	for (size_t i = 0; i < root.children.size();++i) {
		printToken(root.children[i], indent + 1);
	}
}

/**
 * convert TokenType enum value to a string
 * @param type Token type
 * @return corresponding string value, default or unmatching will result
 * empty string
 */
std::string typeToString(TokenType type)
{
	switch (type)
	{
		case (TokenType::Array):
			return "Array";
		case (TokenType::Element):
			return "Element";
		case (TokenType::Identifier):
			return "Identifier";
		case (TokenType::Object):
			return "Object";
		case (TokenType::Value):
			return "Value";
		case (TokenType::Primitive):
			return "Primitive";
		case (TokenType::Null):
			return "Null";
	}
	return "";
}

/**
 * remove leading and trailing white speces (\t\n)
 * @param string need to be trimmed
 * @return new copy of the string without leading and trailing
 * white spaces
 */
std::string trim(std::string_view sv)
{
	const size_t start = sv.find_first_not_of(" \t\n");
	if (start == std::string::npos) {
		return "";
	}
	const size_t end = sv.find_last_not_of(" \t\n");
	return std::string(sv.substr(start, end - start + 1));
}

/**
 * will return the position of a unqoted character, or not surrounded by
 * '"', '{', '[' , '}', ']'
 * usefull to detemine the position of correct position to split
 * {"key1" : "value1"}, {"key2" : "value2", "key3" : "value3"}
 * @param string and the character need to check
 * @return index of the first occuranece of the character in the string
 */
size_t unquotedDelimiter(std::string_view sv, const char c)
{
	size_t	pos					= std::string::npos;
	bool	in_quote			= false;
	int		curly_braces_depth	= 0;
	int		squar_braces_depth	= 0;

	for (size_t i = 0; i < sv.size(); ++i) {
		if (sv[i] == '"' && (i == 0 || sv[i - 1] != '\\')) {
			in_quote = !in_quote;
		}
		if (sv[i] == '{' && !in_quote) {
			curly_braces_depth++;
		}
		if (sv[i] == '}' && !in_quote) {
			curly_braces_depth--;
		}
		if (sv[i] == '[' && !in_quote) {
			squar_braces_depth++;
		}
		if (sv[i] == ']' && !in_quote) {
			squar_braces_depth--;
		}
		if (sv[i] == c
			&& !in_quote
			&& curly_braces_depth == 0
			&& squar_braces_depth == 0
		) {
			pos = i;
			break;
		}
	}
	return pos;
}


/**
 * will return the corresponding token type base on thge string provided
 * @param string need to check
 * @return TokenType enum value base on the format of the string
 */
TokenType getTokenType(const std::string& str)
{
	if (str.empty()) {
		return TokenType::Null;
	}
	if (str.front() == '{' && str.back() == '}') {
		return TokenType::Object;
	}
	if (str.front() == '[' && str.back() == ']') {
		return TokenType::Array;
	}
	if (unquotedDelimiter(str, ':') != std::string::npos) {
		return TokenType::Element;
	}
	if (str.front() == '"' && str.back() == '"') {
		return TokenType::Value;
	}
	return TokenType::Primitive;
}

/**
 * will split a string in to small tokens and created a vector
 * consist of all the tokens for further processing
 * split will happen base on the character ','
 * @param string need to split
 * @return vector filled with tokens
 */
std::vector<std::string> splitElements(std::string_view sv)
{
	std::vector<std::string> tokens;

	if (unquotedDelimiter(sv, ',') != std::string::npos) {
		std::string buffer(sv);
		size_t pos = unquotedDelimiter(buffer, ',');
		while (!buffer.empty() && pos != std::string::npos) {
			tokens.emplace_back(buffer.substr(0, pos));
			buffer = buffer.substr(pos + 1);
			pos = unquotedDelimiter(buffer, ',');
		}
		if (!buffer.empty()) {
			tokens.emplace_back(buffer);
		}
	} else {
		tokens.emplace_back(sv);
	}
	return tokens;
}

/**
 * will create a token of type and return by value, helper function
 * @param string and type of the token needed to create
 * @return token value of Token Type
 */
Token createToken(const std::string& str, TokenType type)
{
	Token token;
	if (type == TokenType::Identifier || type == TokenType::Value) {
		token.type = type;
		token.value = removeQuotes(str);
	} else {
		token = createToken(str);
	}
	return token;
}


/**
 * will create a token of type and return by value, helper function
 * type will be detemind by the function it-self, will call recursivle if
 * string is not a fundamental type (Identifler or Value)
 * @param string the token needed to create
 * @return token value of Toekn Type
 */
Token createToken(const std::string& str)
{
	Token token;
	size_t len = str.length();
	TokenType type = getTokenType(str);

	if (type == TokenType::Object) {
		token.type = TokenType::Object;
		token.value = "";
		std::vector<std::string> values = splitElements(
			trim(std::string(str.substr(1, len - 2)))
		);
		for (auto it : values) {
			if (len > 2) {
				token.children.emplace_back(
					createToken(trim(it))
				);
			}
		}
	}

	if (type == TokenType::Array) {
		token.type = TokenType::Array;
		token.value = "";
		std::vector<std::string> values = splitElements(
			trim(std::string(str.substr(1, len - 2)))
		);
		for (auto it : values) {
			if (len > 2) {
				token.children.emplace_back(
					createToken(trim(it))
				);
			}
		}
	}

	if (type == TokenType::Element) {

		token.type = TokenType::Element;
		size_t pos = unquotedDelimiter(str, ':');

		std::string left = trim(str.substr(0, pos));
		std::string right = trim(str.substr(pos + 1));

		token.children.emplace_back(
			createToken(left, TokenType::Identifier)
		);
		token.children.emplace_back(
			createToken(right)
		);
	}

	if (type == TokenType::Value) {
		token.type = TokenType::Value;
		token.value = removeQuotes(str);
	}

	if (type == TokenType::Primitive) {
		token.type = TokenType::Primitive;
		token.value = str;
	}

	return token;
}

/**
 * will return the value of the key of a json object
 * {"key1" : "value1"} here value of the key is "key1"
 * @param token in the format of string
 * @return value of the key component
 */
std::string getKey(const Token& token)
{
	if (token.type != TokenType::Element) {
		return "";
	}
	if (token.children.empty()) {
		return "";
	}
	const Token& child = token.children.at(0);
	if (child.type != TokenType::Identifier) {
		return "";
	}
	return child.value;
}

/**
 * remove surrounded '"' of a string, helper funtion
 * @param string need to remove the '"' marks
 * @return new string with out any '"' marks
 */
std::string removeQuotes(const std::string& str)
{
	if (str.length() >= 2 && str.front() == '"' && str.back() == '"') {
		return trim(str.substr(1, str.length() - 2));
	}
	return str;
}
