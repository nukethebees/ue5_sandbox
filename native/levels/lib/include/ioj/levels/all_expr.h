#pragma once

#include <ioj/levels/expr_index.h>
#include <ioj/levels/expr_type.h>

#include <vector>

namespace ioj::levels {
struct AllExpr {
    auto operator==(AllExpr const&) const -> bool = default;

    static constexpr ExprType type{ExprType::All};
    std::vector<ExprIndex> children{};
};
}
