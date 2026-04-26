#ifndef ICG_H
#define ICG_H

#include <string>
#include <vector>

// Three-address code instruction
struct TAC {
    std::string result;
    std::string arg1;
    std::string op;
    std::string arg2;
    std::string type; // "assign", "binop", "ifgoto", "goto", "label", "return", "call", "nop"

    std::string toString() const;
};

bool isOperator(const std::string &token);
std::vector<TAC> generateIR(const std::vector<std::string> &originalLines);

#endif
