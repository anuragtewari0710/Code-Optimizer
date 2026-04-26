#include "parser.h"
#include "icg.h"
#include "optimizer.h"
#include <sstream>
#include <unordered_map>
#include <cctype>
#include <algorithm>

using namespace std;

static bool isNumberStr(const string &s) {
    if (s.empty()) return false;
    size_t start = 0;
    if (s[0] == '-') start = 1;
    for (size_t i = start; i < s.size(); i++)
        if (!isdigit(s[i])) return false;
    return true;
}

static string trim(const string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n;");
    return s.substr(a, b - a + 1);
}

// Rebuild optimized source lines from the after-IR
// We keep non-assignment lines (preprocessor, braces, function signatures) as-is
static vector<string> reconstructSource(
    const vector<string> &originalLines,
    const vector<TAC> &afterIR)
{
    // Build a map from variable -> optimized value from the IR
    unordered_map<string, string> varMap;
    for (auto &ins : afterIR) {
        if (ins.type == "assign")
            varMap[ins.result] = ins.arg1;
        else if (ins.type == "binop")
            varMap[ins.result] = ins.arg1 + " " + ins.op + " " + ins.arg2;
    }

    vector<string> out;
    for (auto &line : originalLines) {
        string trimmed = trim(line);

        // Pass-through lines
        if (trimmed.empty() || trimmed[0] == '#' ||
            trimmed == "{" || trimmed == "}" ||
            trimmed.find("using ") != string::npos ||
            trimmed.find("int main") != string::npos ||
            trimmed.find("void ") != string::npos ||
            trimmed.find("return") != string::npos ||
            trimmed.find("cout") != string::npos ||
            trimmed.find("cin") != string::npos ||
            trimmed.find("for") != string::npos ||
            trimmed.find("while") != string::npos ||
            trimmed.find("if") != string::npos) {
            out.push_back(line);
            continue;
        }

        // Assignment lines: rewrite RHS if optimized
        size_t eqPos = string::npos;
        for (size_t i = 0; i < line.size(); i++) {
            if (line[i] == '=' &&
                (i == 0 || (line[i-1] != '!' && line[i-1] != '<' && line[i-1] != '>' && line[i-1] != '=')) &&
                (i + 1 >= line.size() || line[i+1] != '=')) {
                eqPos = i;
                break;
            }
        }

        if (eqPos != string::npos) {
            string lhs = trim(line.substr(0, eqPos));
            // Extract variable name from lhs (strip type)
            istringstream lhss(lhs);
            string w1, w2;
            lhss >> w1;
            string varName;
            if (lhss >> w2) varName = w2;
            else varName = w1;

            // Get indentation
            string indent = "";
            for (char c : line) { if (c == ' ' || c == '\t') indent += c; else break; }

            if (varMap.count(varName)) {
                out.push_back(indent + lhs + " = " + varMap[varName] + ";");
            } else {
                out.push_back(line);
            }
            continue;
        }

        out.push_back(line);
    }
    return out;
}

ParseResult optimizeExpressions(const vector<string> &tokens,
                                  const vector<string> &originalLines)
{
    ParseResult result;

    // Step 1: Generate IR
    vector<TAC> rawIR = generateIR(originalLines);
    for (auto &ins : rawIR)
        result.irBefore.push_back(ins.toString());

    // Step 2: Optimize IR
    OptimizationResult opt = optimize(rawIR);
    result.optimizationLog = opt.log;

    for (auto &ins : opt.afterIR)
        result.irAfter.push_back(ins.toString());

    // Step 3: Reconstruct source
    result.optimizedLines = reconstructSource(originalLines, opt.afterIR);

    return result;
}
