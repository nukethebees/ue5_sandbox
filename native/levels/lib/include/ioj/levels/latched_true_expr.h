#pragma once
#include <ioj/expr_kind.h>

#include <cstdint>
namespace ioj::levels {
struct LatchedTrueExpr {
    auto operator==(LatchedTrueExpr const&) const -> bool = default;
    static constexpr ExprKind type{ExprKind::LatchedTrue};
    std::uint32_t latch{};
};
}
