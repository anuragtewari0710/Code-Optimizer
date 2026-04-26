#include "parser.h"
#include "lexer.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
using namespace std;

// Escape a string for JSON
static string jsonEscape(const string &s) {
    string out;
    for (char c : s) {
        if (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

static string jsonArray(const vector<string> &v) {
    string out = "[";
    for (size_t i = 0; i < v.size(); i++) {
        out += "\"" + jsonEscape(v[i]) + "\"";
        if (i + 1 < v.size()) out += ",";
    }
    out += "]";
    return out;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cout << "Usage: " << argv[0] << " input.cpp output.json\n";
        return 1;
    }

    string inputFile  = argv[1];
    string outputFile = argv[2];

    ifstream in(inputFile);
    if (!in) { cerr << "Cannot open input file\n"; return 1; }

    string code, line;
    vector<string> originalLines;
    while (getline(in, line)) {
        code += line + "\n";
        originalLines.push_back(line);
    }
    in.close();

    vector<string> tokens = tokenize(code);
    ParseResult pr = optimizeExpressions(tokens, originalLines);

    // Build JSON output
    string json = "{\n";
    json += "  \"optimizedCode\": " + jsonArray(pr.optimizedLines) + ",\n";
    json += "  \"irBefore\": "      + jsonArray(pr.irBefore)       + ",\n";
    json += "  \"irAfter\": "       + jsonArray(pr.irAfter)        + ",\n";
    json += "  \"optimizationLog\": " + jsonArray(pr.optimizationLog) + "\n";
    json += "}\n";

    ofstream out(outputFile);
    if (!out) { cerr << "Cannot write to output file\n"; return 1; }
    out << json;
    out.close();

    cout << "Optimization complete! Output written to " << outputFile << "\n";
    return 0;
}
