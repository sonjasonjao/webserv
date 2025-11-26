#include "JSON.hpp"

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

std::string trim(std::string_view sv) {
    const size_t start = sv.find_first_not_of(" \t\n");
    if(start == std::string::npos) {
        return ("");
    }
    const size_t end = sv.find_last_not_of(" \t\n");
    return (std::string(sv.substr(start, end - start + 1)));
}

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

Token create_token(const std::string& str, TokenType type) {
    Token token;
    if(type == TokenType::Identifier || type == TokenType::Value) {
        token.type = type;
        token.value = str;
    } else {
        token = create_token(str);
    }
    return (token);
}

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
        token.value = str;
    }

    return (token);
}

