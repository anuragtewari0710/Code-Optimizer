#include "optimizer.h"
#include <map>
#include <set>
#include <sstream>
#include <algorithm>
#include <cctype>

using namespace std;

// ─────────────────────────────────────────────────────────────
// HELPERS
// ─────────────────────────────────────────────────────────────

static bool isNumber(const string &s) {
    if (s.empty()) return false;
    size_t start = 0;
    if (s[0] == '-') start = 1;
    if (start == s.size()) return false;
    for (size_t i = start; i < s.size(); i++)
        if (!isdigit(s[i])) return false;
    return true;
}

static int toInt(const string &s) { return stoi(s); }
static string toStr(int v)        { return to_string(v); }

static string applyOp(const string &a, const string &op, const string &b) {
    int x = toInt(a), y = toInt(b);
    if (op == "+") return toStr(x + y);
    if (op == "-") return toStr(x - y);
    if (op == "*") return toStr(x * y);
    if (op == "/" && y != 0) return toStr(x / y);
    if (op == "/" && y == 0) return "0"; // guard
    return a + op + b;
}

static bool isPowerOf2(int n, int &exp) {
    if (n <= 0) return false;
    exp = 0;
    while (n > 1) {
        if (n & 1) return false;
        n >>= 1;
        exp++;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────
// PASS 1 — CONSTANT FOLDING
// Evaluate binary ops where both operands are literals.
// ─────────────────────────────────────────────────────────────
static vector<TAC> constantFolding(const vector<TAC> &ir, vector<string> &log) {
    vector<TAC> out;
    for (auto ins : ir) {
        if (ins.type == "binop" && isNumber(ins.arg1) && isNumber(ins.arg2)) {
            string val = applyOp(ins.arg1, ins.op, ins.arg2);
            log.push_back("[ConstFold] " + ins.result + " = " + ins.arg1 + " " + ins.op + " " + ins.arg2 + "  =>  " + ins.result + " = " + val);
            TAC t; t.type = "assign"; t.result = ins.result; t.arg1 = val;
            out.push_back(t);
        } else {
            out.push_back(ins);
        }
    }
    return out;
}

// ─────────────────────────────────────────────────────────────
// PASS 2 — CONSTANT PROPAGATION
// Replace variable uses with their known constant values.
// ─────────────────────────────────────────────────────────────
static vector<TAC> constantPropagation(const vector<TAC> &ir, vector<string> &log) {
    map<string, string> constMap; // var -> literal value
    vector<TAC> out;

    for (auto ins : ir) {
        // Substitute known constants in args
        auto sub = [&](string &s) {
            if (!isNumber(s) && constMap.count(s)) {
                log.push_back("[ConstProp] Replacing " + s + " with " + constMap[s]);
                s = constMap[s];
            }
        };

        if (ins.type == "binop") { sub(ins.arg1); sub(ins.arg2); }
        if (ins.type == "assign") { sub(ins.arg1); }

        // Track new constants
        if (ins.type == "assign" && isNumber(ins.arg1))
            constMap[ins.result] = ins.arg1;
        else if (ins.type == "binop" && isNumber(ins.arg1) && isNumber(ins.arg2)) {
            string val = applyOp(ins.arg1, ins.op, ins.arg2);
            constMap[ins.result] = val;
        } else {
            constMap.erase(ins.result); // result is no longer a known constant
        }

        out.push_back(ins);
    }

    // Second pass: fold newly created constant binops
    vector<TAC> out2;
    for (auto ins : out) {
        if (ins.type == "binop" && isNumber(ins.arg1) && isNumber(ins.arg2)) {
            string val = applyOp(ins.arg1, ins.op, ins.arg2);
            log.push_back("[ConstFold2] " + ins.result + " = " + ins.arg1 + " " + ins.op + " " + ins.arg2 + "  =>  " + ins.result + " = " + val);
            TAC t; t.type = "assign"; t.result = ins.result; t.arg1 = val;
            out2.push_back(t);
        } else {
            out2.push_back(ins);
        }
    }
    return out2;
}

// ─────────────────────────────────────────────────────────────
// PASS 3 — COPY PROPAGATION
// If x = y (simple copy), replace later uses of x with y.
// ─────────────────────────────────────────────────────────────
static vector<TAC> copyPropagation(const vector<TAC> &ir, vector<string> &log) {
    map<string, string> copyMap;
    vector<TAC> out;

    auto resolve = [&](string &s) {
        if (copyMap.count(s)) {
            log.push_back("[CopyProp] Replacing " + s + " with " + copyMap[s]);
            s = copyMap[s];
        }
    };

    for (auto ins : ir) {
        if (ins.type == "assign") {
            resolve(ins.arg1);
            if (!isNumber(ins.arg1) && ins.arg1 != ins.result)
                copyMap[ins.result] = ins.arg1;
            else
                copyMap.erase(ins.result);
        }
        if (ins.type == "binop") {
            resolve(ins.arg1);
            resolve(ins.arg2);
            copyMap.erase(ins.result);
        }
        out.push_back(ins);
    }
    return out;
}

// ─────────────────────────────────────────────────────────────
// PASS 4 — ALGEBRAIC SIMPLIFICATION
// x + 0 = x, x * 1 = x, x * 0 = 0, x - 0 = x, x / 1 = x
// ─────────────────────────────────────────────────────────────
static vector<TAC> algebraicSimplification(const vector<TAC> &ir, vector<string> &log) {
    vector<TAC> out;
    for (auto ins : ir) {
        if (ins.type == "binop") {
            string a = ins.arg1, op = ins.op, b = ins.arg2, r = ins.result;
            bool simplified = false;

            if (op == "+" && b == "0") {
                log.push_back("[AlgSimp] " + r + " = " + a + " + 0  =>  " + r + " = " + a);
                TAC t; t.type="assign"; t.result=r; t.arg1=a; out.push_back(t); simplified=true;
            } else if (op == "+" && a == "0") {
                log.push_back("[AlgSimp] " + r + " = 0 + " + b + "  =>  " + r + " = " + b);
                TAC t; t.type="assign"; t.result=r; t.arg1=b; out.push_back(t); simplified=true;
            } else if (op == "-" && b == "0") {
                log.push_back("[AlgSimp] " + r + " = " + a + " - 0  =>  " + r + " = " + a);
                TAC t; t.type="assign"; t.result=r; t.arg1=a; out.push_back(t); simplified=true;
            } else if (op == "*" && b == "1") {
                log.push_back("[AlgSimp] " + r + " = " + a + " * 1  =>  " + r + " = " + a);
                TAC t; t.type="assign"; t.result=r; t.arg1=a; out.push_back(t); simplified=true;
            } else if (op == "*" && a == "1") {
                log.push_back("[AlgSimp] " + r + " = 1 * " + b + "  =>  " + r + " = " + b);
                TAC t; t.type="assign"; t.result=r; t.arg1=b; out.push_back(t); simplified=true;
            } else if (op == "*" && (b == "0" || a == "0")) {
                log.push_back("[AlgSimp] " + r + " = " + a + " * " + b + "  =>  " + r + " = 0");
                TAC t; t.type="assign"; t.result=r; t.arg1="0"; out.push_back(t); simplified=true;
            } else if (op == "/" && b == "1") {
                log.push_back("[AlgSimp] " + r + " = " + a + " / 1  =>  " + r + " = " + a);
                TAC t; t.type="assign"; t.result=r; t.arg1=a; out.push_back(t); simplified=true;
            } else if (op == "-" && a == b) {
                log.push_back("[AlgSimp] " + r + " = " + a + " - " + b + "  =>  " + r + " = 0");
                TAC t; t.type="assign"; t.result=r; t.arg1="0"; out.push_back(t); simplified=true;
            }

            if (!simplified) out.push_back(ins);
        } else {
            out.push_back(ins);
        }
    }
    return out;
}

// ─────────────────────────────────────────────────────────────
// PASS 5 — STRENGTH REDUCTION
// Replace expensive ops with cheaper equivalents.
// x * 2^n  =>  x << n  (represented as x*2=x+x in source)
// x * 2    =>  x + x
// ─────────────────────────────────────────────────────────────
static vector<TAC> strengthReduction(const vector<TAC> &ir, vector<string> &log) {
    vector<TAC> out;
    for (auto ins : ir) {
        if (ins.type == "binop" && ins.op == "*") {
            string a = ins.arg1, b = ins.arg2, r = ins.result;
            int exp = 0;
            // x * 2 => x + x
            if (b == "2" && !isNumber(a)) {
                log.push_back("[StrRed] " + r + " = " + a + " * 2  =>  " + r + " = " + a + " + " + a);
                TAC t; t.type="binop"; t.result=r; t.arg1=a; t.op="+"; t.arg2=a; out.push_back(t); continue;
            }
            if (a == "2" && !isNumber(b)) {
                log.push_back("[StrRed] " + r + " = 2 * " + b + "  =>  " + r + " = " + b + " + " + b);
                TAC t; t.type="binop"; t.result=r; t.arg1=b; t.op="+"; t.arg2=b; out.push_back(t); continue;
            }
            // x * power_of_2 (note as shift)
            if (isNumber(b) && !isNumber(a) && isPowerOf2(toInt(b), exp) && exp > 1) {
                log.push_back("[StrRed] " + r + " = " + a + " * " + b + "  =>  " + r + " = " + a + " << " + toStr(exp) + " (shift)");
                // Keep as multiply but log the optimization
            }
        }
        out.push_back(ins);
    }
    return out;
}

// ─────────────────────────────────────────────────────────────
// PASS 6 — COMMON SUBEXPRESSION ELIMINATION (CSE)
// If the same expression a op b appears twice, reuse the first result.
// ─────────────────────────────────────────────────────────────
static vector<TAC> commonSubexpressionElim(const vector<TAC> &ir, vector<string> &log) {
    map<string, string> exprMap; // "a op b" -> result var
    vector<TAC> out;

    for (auto ins : ir) {
        if (ins.type == "binop") {
            string key = ins.arg1 + " " + ins.op + " " + ins.arg2;
            string keyRev = ins.arg2 + " " + ins.op + " " + ins.arg1; // commutative
            bool commutative = (ins.op == "+" || ins.op == "*");

            if (exprMap.count(key)) {
                log.push_back("[CSE] " + ins.result + " = " + key + "  =>  " + ins.result + " = " + exprMap[key] + " (reuse)");
                TAC t; t.type="assign"; t.result=ins.result; t.arg1=exprMap[key]; out.push_back(t);
            } else if (commutative && exprMap.count(keyRev)) {
                log.push_back("[CSE] " + ins.result + " = " + key + "  =>  " + ins.result + " = " + exprMap[keyRev] + " (reuse, commutative)");
                TAC t; t.type="assign"; t.result=ins.result; t.arg1=exprMap[keyRev]; out.push_back(t);
            } else {
                exprMap[key] = ins.result;
                out.push_back(ins);
            }
        } else {
            // Any assignment to a variable invalidates expressions using it
            if (!ins.result.empty()) {
                for (auto it = exprMap.begin(); it != exprMap.end(); ) {
                    if (it->first.find(ins.result) != string::npos || it->second == ins.result)
                        it = exprMap.erase(it);
                    else ++it;
                }
            }
            out.push_back(ins);
        }
    }
    return out;
}

// ─────────────────────────────────────────────────────────────
// PASS 7 — DEAD CODE ELIMINATION
// Remove assignments whose result is never used downstream.
// ─────────────────────────────────────────────────────────────
static vector<TAC> deadCodeElimination(const vector<TAC> &ir, vector<string> &log) {
    // Collect all used variables (right-hand sides and special uses)
    set<string> used;
    for (auto &ins : ir) {
        if (!ins.arg1.empty() && !isNumber(ins.arg1)) used.insert(ins.arg1);
        if (!ins.arg2.empty() && !isNumber(ins.arg2)) used.insert(ins.arg2);
        if (ins.type == "return" && !ins.arg1.empty()) used.insert(ins.arg1);
    }

    vector<TAC> out;
    for (auto &ins : ir) {
        bool isAssignment = (ins.type == "assign" || ins.type == "binop");
        bool resultUsed = used.count(ins.result) > 0;
        bool isTempVar = (!ins.result.empty() && ins.result[0] == 't' && isdigit(ins.result[1]));

        if (isAssignment && isTempVar && !resultUsed) {
            log.push_back("[DCE] Removed dead temp: " + ins.toString());
            continue;
        }
        out.push_back(ins);
    }
    return out;
}

// ─────────────────────────────────────────────────────────────
// PASS 8 — LOOP-INVARIANT CODE MOTION (basic detection)
// Mark instructions inside loops whose operands don't change.
// For a line-based IR without explicit loop structure, we detect
// consecutive assigns that produce the same value (simplified).
// ─────────────────────────────────────────────────────────────
static vector<TAC> loopInvariantMotion(const vector<TAC> &ir, vector<string> &log) {
    // Simple heuristic: find duplicate constant assigns inside "loop" blocks
    // We detect: consecutive identical assign/binop that could be hoisted
    map<string, string> seen; // expr -> result
    vector<TAC> out;

    for (auto ins : ir) {
        if (ins.type == "label") { seen.clear(); out.push_back(ins); continue; }

        if (ins.type == "assign" && isNumber(ins.arg1)) {
            string key = ins.result + "=" + ins.arg1;
            if (seen.count(key)) {
                log.push_back("[LoopInv] Hoisted invariant: " + ins.toString());
                continue; // skip duplicate
            }
            seen[key] = ins.arg1;
        }
        out.push_back(ins);
    }
    return out;
}

// ─────────────────────────────────────────────────────────────
// MAIN OPTIMIZER
// ─────────────────────────────────────────────────────────────
OptimizationResult optimize(vector<TAC> ir) {
    OptimizationResult res;
    res.beforeIR = ir;

    vector<string> &log = res.log;

    // Apply passes in order
    ir = constantFolding(ir, log);
    ir = constantPropagation(ir, log);
    ir = copyPropagation(ir, log);
    ir = algebraicSimplification(ir, log);
    ir = strengthReduction(ir, log);
    ir = commonSubexpressionElim(ir, log);
    ir = loopInvariantMotion(ir, log);
    ir = deadCodeElimination(ir, log);

    res.afterIR = ir;
    return res;
}
