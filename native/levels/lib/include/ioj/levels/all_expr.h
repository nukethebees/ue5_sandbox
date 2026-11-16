#pragma once

#include <ioj/expr_kind.h>
#include <ioj/levels/expr_index.h>

#include <vector>

namespace ioj::levels {
struct AllExpr {
    auto operator==(AllExpr const&) const -> bool = default;

    static constexpr ExprKind type{ExprKind::All};
    std::vector<ExprIndex> children{};
};
}
