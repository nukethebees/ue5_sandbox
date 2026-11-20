#pragma once

#include <ioj/expr_kind.h>
#include <ioj/levels/logic_indices.h>

#include <vector>

namespace ioj::levels {
struct AnyExpr {
    auto operator==(AnyExpr const&) const -> bool = default;

    static constexpr auto type() -> ExprKind { return ExprKind::Any; }
    std::vector<ExprIndex> children{};
};
}
