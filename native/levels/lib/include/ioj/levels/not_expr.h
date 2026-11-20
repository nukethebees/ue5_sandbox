#pragma once
#include <ioj/expr_kind.h>
#include <ioj/levels/expr_index.h>

#include <vector>
namespace ioj::levels {
struct NotExpr {
    auto operator==(NotExpr const&) const -> bool = default;
    static constexpr auto type() -> ExprKind { return ExprKind::Not; }
    std::vector<ExprIndex> children;
};
}
