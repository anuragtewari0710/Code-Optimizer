#include "icg.h"
#include <sstream>
#include <cctype>
#include <algorithm>

bool isOperator(const std::string &token) {
    return token == "+" || token == "-" || token == "*" || token == "/";
}

static bool isNumberStr(const std::string &s) {
    if (s.empty()) return false;
    size_t start = 0;
    if (s[0] == '-') start = 1;
    for (size_t i = start; i < s.size(); i++)
        if (!isdigit(s[i])) return false;
    return true;
}

static int tempCount = 0;
static std::string newTemp() {
    return "t" + std::to_string(tempCount++);
}

static std::string trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n;");
    return s.substr(a, b - a + 1);
}

// Parse a simple expression "a op b" or just "a"
// Returns TAC instructions and the result variable
static std::string parseExpr(const std::string &expr, std::vector<TAC> &instrs) {
    std::string e = trim(expr);

    // Tokenize the expression: split on operators, keep operands
    // Handles spaces: "2 + 3", "a * b", "x+y"
    std::string ops = "+-*/";
    
    // Find first binary operator (look for operator surrounded by operands)
    for (size_t i = 1; i < e.size(); i++) {
        char c = e[i];
        if (c == '+' || c == '-' || c == '*' || c == '/') {
            // Check that left side is non-empty operand and right side is non-empty
            std::string left = trim(e.substr(0, i));
            std::string right = trim(e.substr(i + 1));
            std::string op(1, c);
            
            // Skip if this looks like a sign (e.g. unary minus at start)
            if (left.empty()) continue;
            // Skip if right is empty
            if (right.empty()) continue;
            
            // Check left ends with alnum/underscore
            if (!isalnum(left.back()) && left.back() != '_') continue;
            // Check right starts with alnum, underscore, or digit
            if (!isalnum(right[0]) && right[0] != '_') continue;
            
            // We have a valid binary operation
            std::string t = newTemp();
            TAC tac;
            tac.result = t;
            tac.arg1 = left;
            tac.op = op;
            tac.arg2 = right;
            tac.type = "binop";
            instrs.push_back(tac);
            return t;
        }
    }
    return e; // atomic (variable or literal)
}

std::vector<TAC> generateIR(const std::vector<std::string> &originalLines) {
    tempCount = 0;
    std::vector<TAC> ir;
    int labelCount = 0;

    for (size_t lineIdx = 0; lineIdx < originalLines.size(); lineIdx++) {
        std::string line = trim(originalLines[lineIdx]);
        if (line.empty() || line[0] == '#') continue;

        // for loop: for(init; cond; incr)
        if (line.substr(0, 3) == "for") {
            TAC lbl;
            lbl.type = "label";
            lbl.result = "L" + std::to_string(labelCount++);
            ir.push_back(lbl);
            continue;
        }

        // while loop
        if (line.substr(0, 5) == "while") {
            TAC lbl;
            lbl.type = "label";
            lbl.result = "L" + std::to_string(labelCount++);
            ir.push_back(lbl);
            continue;
        }

        // return statement
        if (line.substr(0, 6) == "return") {
            std::string val = trim(line.substr(6));
            if (!val.empty() && val.back() == ';') val.pop_back();
            TAC tac;
            tac.type = "return";
            tac.arg1 = val;
            ir.push_back(tac);
            continue;
        }

        // Assignment: detect "=" that is not "==" or "!=" or "<=" or ">="
        size_t eqPos = std::string::npos;
        for (size_t i = 0; i < line.size(); i++) {
            if (line[i] == '=' &&
                (i == 0 || (line[i-1] != '!' && line[i-1] != '<' && line[i-1] != '>' && line[i-1] != '=')) &&
                (i + 1 >= line.size() || line[i+1] != '=')) {
                eqPos = i;
                break;
            }
        }

        if (eqPos != std::string::npos) {
            std::string lhs = trim(line.substr(0, eqPos));
            std::string rhs = trim(line.substr(eqPos + 1));
            if (!rhs.empty() && rhs.back() == ';') rhs.pop_back();
            rhs = trim(rhs);

            // Strip type keyword from lhs (int, float, double, etc.)
            std::istringstream lhss(lhs);
            std::string w1, w2;
            lhss >> w1;
            if (lhss >> w2) lhs = w2; // "int x" -> lhs = "x"
            else lhs = w1;

            std::string rhsResult = parseExpr(rhs, ir);

            TAC tac;
            tac.type = "assign";
            tac.result = lhs;
            tac.arg1 = rhsResult;
            ir.push_back(tac);
            continue;
        }

        // Standalone expression / function call / other
        if (!line.empty() && line != "{" && line != "}") {
            TAC tac;
            tac.type = "nop";
            tac.arg1 = line;
            ir.push_back(tac);
        }
    }
    return ir;
}

std::string TAC::toString() const {
    if (type == "binop")   return result + " = " + arg1 + " " + op + " " + arg2;
    if (type == "assign")  return result + " = " + arg1;
    if (type == "ifgoto")  return "if " + arg1 + " goto " + result;
    if (type == "goto")    return "goto " + result;
    if (type == "label")   return result + ":";
    if (type == "return")  return "return " + arg1;
    if (type == "nop")     return "; " + arg1;
    return "";
}
