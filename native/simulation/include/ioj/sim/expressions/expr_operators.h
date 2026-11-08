#pragma once

#include <ioj/sim/expressions/composite_kind.h>
#include <ioj/sim/expressions/expr_composite_index.h>
#include <ioj/sim/expressions/expr_handle.h>
#include <ioj/sim/index_span.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace ioj::sim {
struct ExprOperators {

    std::vector<std::uint8_t> results;
    std::vector<CompositeKind> kinds;
    std::vector<IndexSpan> child_ranges;
    std::vector<ExprHandle> children;
};
}
