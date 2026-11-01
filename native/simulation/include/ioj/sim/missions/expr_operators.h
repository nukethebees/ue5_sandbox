#pragma once

#include <ioj/levels/expr_result.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/missions/composite_kind.h>
#include <ioj/sim/missions/expr_handle.h>

#include <vector>

namespace ioj::sim {
struct ExprOperators {
    std::vector<ExprResult> results;
    std::vector<CompositeKind> kinds;
    std::vector<IndexSpan> child_ranges;
    std::vector<ExprHandle> children;
};
}
