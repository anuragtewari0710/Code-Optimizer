#include "lexer.h"
#include <string>
#include <vector>
#include <cctype>

std::vector<std::string> tokenize(const std::string &code) {
    std::vector<std::string> tokens;
    std::string token;
    for (size_t i = 0; i < code.size(); i++) {
        char c = code[i];
        if (isspace(c)) continue;
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '=' ||
            c == '(' || c == ')' || c == ';' || c == '<' || c == '>' ||
            c == '{' || c == '}' || c == ',' || c == '!' || c == '&' || c == '|') {
            if (!token.empty()) { tokens.push_back(token); token.clear(); }
            tokens.push_back(std::string(1, c));
        } else if (isalnum(c) || c == '_') {
            token += c;
        } else {
            if (!token.empty()) { tokens.push_back(token); token.clear(); }
            tokens.push_back(std::string(1, c));
        }
    }
    if (!token.empty()) tokens.push_back(token);
    return tokens;
}