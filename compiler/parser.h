#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>
#include "optimizer.h"

struct ParseResult {
    std::vector<std::string> optimizedLines;
    std::vector<std::string> irBefore;    // IR before optimization
    std::vector<std::string> irAfter;     // IR after optimization
    std::vector<std::string> optimizationLog;
};

ParseResult optimizeExpressions(const std::vector<std::string> &tokens,
                                 const std::vector<std::string> &originalLines);

#endif
