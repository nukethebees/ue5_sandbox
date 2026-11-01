#pragma once

#include <ioj/levels/expr_index.h>
#include <ioj/levels/expr_type.h>

#include <vector>

namespace ioj::levels {
struct AnyExpr {
    auto operator==(AnyExpr const&) const -> bool = default;

    static constexpr ExprType type{ExprType::Any};
    std::vector<ExprIndex> children{};
};
}
