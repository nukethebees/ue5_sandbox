#pragma once

#include <codegen/ast/expr_kind.h>

#include <cstdint>

namespace lispb::codegen {

struct Expr {
    inline static constexpr std::uint32_t index_mask{0x00ffffffu};
    inline static constexpr std::uint32_t kind_shift{24};

    std::uint32_t value{};
    explicit operator bool() const { return value != 0; }
    auto kind() const -> ExprKind { return static_cast<ExprKind>(value >> kind_shift); }
    auto index() const -> std::uint32_t { return value & index_mask; }
    auto operator==(Expr const&) const -> bool = default;
};

static_assert(sizeof(Expr) == 4);

} // namespace lispb::codegen
