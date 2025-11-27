#include "JSON.hpp"

/**
 * this function will print the AST recursively, pure debugging function
*/
void print_token(const Token& root, int indent) {
    for(int i = 0; i < indent; ++i){
        std::cout << " ";
    }
    std::cout << type_to_string(root.type);
    if(!root.value.empty()) {
        std::cout << "(" << root.value << ")";
    }
    std::cout << "\n";
    for(size_t i = 0; i < root.children.size();++i) {
        print_token(root.children[i], indent + 1);
    }
}

/**
 * convert TokenType enum value to a string
 * @param type Token type
 * @return corresponding string value, default or umatching will result
 * empty string
*/
std::string type_to_string(TokenType type) {
    switch (type)
    {
        case (TokenType::Array):
            return ("Array");
        case (TokenType::Element):
            return ("Element");
        case (TokenType::Identifier):
            return ("Identifier");
        case (TokenType::Object):
            return ("Object");
        case (TokenType::Value):
            return ("Value");
        case (TokenType::Null):
            return ("Null");
    }
    return ("");
}

/**
 * remove leading and trailing white speces (\t\n)
 * @param string need to be trimmed
 * @return new copy of the string without leading and trailing
 * white spaces
*/
std::string trim(std::string_view sv) {
    const size_t start = sv.find_first_not_of(" \t\n");
    if(start == std::string::npos) {
        return ("");
    }
    const size_t end = sv.find_last_not_of(" \t\n");
    return (std::string(sv.substr(start, end - start + 1)));
}

/**
 * will return the position of a unqoted character, or not surrounded by
 * '"', '{', '[' , '}', ']'
 * usefull to detemine the position of correct position to split
 * {"key1" : "value1"}, {"key2" : "value2", "key3" : "value3"}
 * @param string and the character need to check
 * @return index of the first occuranece of the character in the string
*/
size_t unquoted_delimiter(std::string_view sv, const char c) {
    size_t pos = std::string::npos;
    bool in_quote       = false;
    int curly_braces_depth    = 0;
    int squar_braces_depth    = 0;
    for(size_t i = 0; i < sv.size(); ++i) {
        if(sv[i] == '"'  && (i == 0 || sv[i - 1] != '\\')){
            in_quote = !in_quote;
        }
        if(sv[i] == '{'){
            curly_braces_depth++;
        }
        if(sv[i] == '}'){
            curly_braces_depth--;
        }
        if(sv[i] == '['){
            squar_braces_depth++;
        }
        if(sv[i] == ']'){
            squar_braces_depth--;
        }
        if(sv[i] == c
            && !in_quote
            && curly_braces_depth == 0
            && squar_braces_depth == 0
        ){
            pos = i;
            break;
        }
    }
    return (pos);
}


/**
 * will return the corresponding token type base on thge string provided
 * @param string need to check
 * @return TokenType enum value base on the format of the string
*/
TokenType get_token_type(const std::string& str) {
    if(str.empty()) {
        return (TokenType::Null);
    }
    size_t len = str.length();
    if(str[0] == '{' && str[len - 1] == '}') {
        return (TokenType::Object);
    }
    if(str[0] == '[' && str[len - 1] == ']') {
        return (TokenType::Array);
    }
    if(unquoted_delimiter(str, ':') != std::string::npos) {
        return (TokenType::Element);
    }
    if(str[0] == '"' && str[len - 1] == '"') {
        return (TokenType::Value);
    }
    return (TokenType::Identifier);
}

/**
 * will split a string in to small tokens and created a vector
 * consist of all the tokens for further processing
 * split will happen base on the character ','
 * @param string need to split
 * @return vector filled with tokens
*/
std::vector<std::string> split_elements(const std::string& str) {

    std::vector<std::string> tokens;

    if(unquoted_delimiter(str, ',') != std::string::npos) {
        std::string buffer(str);
        size_t pos = unquoted_delimiter(buffer, ',');
        while(!buffer.empty() && pos != std::string::npos) {
            tokens.emplace_back(buffer.substr(0, pos));
            buffer = buffer.substr(pos + 1);
            pos = unquoted_delimiter(buffer, ',');
        }
        if(!buffer.empty()){
            tokens.emplace_back(buffer);
        }
    } else {
        tokens.emplace_back(str);
    }
    return (tokens);
}

/**
 * will create a token of type and return by value, helper fucntion
 * @param string and type of the token needed to create
 * @return token value of Toekn Type
*/
Token create_token(const std::string& str, TokenType type) {
    Token token;
    if(type == TokenType::Identifier || type == TokenType::Value) {
        token.type = type;
        token.value = remove_quotes(str);
    } else {
        token = create_token(str);
    }
    return (token);
}


/**
 *
 * will create a token of type and return by value, helper fucntion
 * type will be detemind by the function it-self, will call recursivle if
 * string is not a fundamental type (Identifler or Value)
 * @param string the token needed to create
 * @return token value of Toekn Type
*/
Token create_token(const std::string& str) {
    Token token;
    size_t len = str.length();
    TokenType type = get_token_type(str);

    if(type == TokenType::Object) {
        token.type = TokenType::Object;
        token.value = "";
        std::vector<std::string> values = split_elements(
            trim(std::string(str.substr(1, len - 2)))
        );
        for(auto it : values) {
            if(len > 2) {
                token.children.emplace_back(
                    create_token(trim(it))
                );
            }
        }
    }

    if(type == TokenType::Array) {
        token.type = TokenType::Array;
        token.value = "";
        std::vector<std::string> values = split_elements(
            trim(std::string(str.substr(1, len - 2)))
        );
        for(auto it : values) {
            if(len > 2) {
                token.children.emplace_back(
                    create_token(trim(it))
                );
            }
        }
    }

    if(type == TokenType::Element) {

        token.type = TokenType::Element;
        size_t pos = unquoted_delimiter(str, ':');

        std::string left = trim(str.substr(0, pos));
        std::string right = trim(str.substr(pos + 1));

        token.children.emplace_back(
            create_token(left, TokenType::Identifier)
        );
        token.children.emplace_back(
            create_token(right)
        );
    }

    if(type == TokenType::Value) {
        token.type = TokenType::Value;
        token.value = remove_quotes(str);
    }

    return (token);
}

/**
 * will return the value of the key of a json object
 * {"key1" : "value1"} here value of the key is "key1"
 * @param token in the format of string
 * @return value of the key component
*/
std::string get_key(const Token& token) {
    if(token.type == TokenType::Element
        && !token.children.empty()) {
            if(token.children.at(0).type == TokenType::Identifier) {
                return (token.children.at(0).value);
            }
    }
    return ("");
}

/**
 * remove surrounded '"' of a string, helper funtion
 * @param string need to remove the '"' marks
 * @return new string with out any '"' marks
*/
std::string remove_quotes(const std::string& str) {
    if(str.length() >= 2 && str.front() == '"' && str.back() == '"') {
        return (trim(str.substr(1, str.length() - 2)));
    }
    return (str);
}
