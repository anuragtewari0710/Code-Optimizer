#ifndef OPTIMIZER_H
#define OPTIMIZER_H
#include <vector>
#include <string>
#include "icg.h"

struct OptimizationResult {
    std::vector<TAC> beforeIR;    // IR before optimization
    std::vector<TAC> afterIR;     // IR after optimization
    std::vector<std::string> log; // What was applied
};

OptimizationResult optimize(std::vector<TAC> ir);

#endif
