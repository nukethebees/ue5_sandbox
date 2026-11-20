#pragma once

#include <ioj/expr_kind.h>
#include <ioj/levels/logic_indices.h>

#include <vector>

namespace ioj::levels {
struct AllExpr {
    auto operator==(AllExpr const&) const -> bool = default;

    static constexpr auto type() -> ExprKind { return ExprKind::All; }
    std::vector<ExprIndex> children{};
};
}
