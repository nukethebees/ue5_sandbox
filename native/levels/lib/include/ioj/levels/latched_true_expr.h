#pragma once
#include <ioj/levels/expr_type.h>

#include <cstdint>
namespace ioj::levels {
struct LatchedTrueExpr {
    auto operator==(LatchedTrueExpr const&) const -> bool = default;
    static constexpr ExprType type{ExprType::LatchedTrue};
    std::uint32_t latch{};
};
}
