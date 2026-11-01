#pragma once

#include <ioj/levels/expr_result.h>
#include <ioj/levels/expr_type.h>
#include <ioj/sim/index_span.h>

#include <vector>

namespace ioj::sim {
struct ExprOperators {
    std::vector<ExprResult> results;
    std::vector<ExprType> types;
    std::vector<IndexSpan> child_ranges;
};
}
