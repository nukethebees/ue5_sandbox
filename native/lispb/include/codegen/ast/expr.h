#pragma once

#include <codegen/ast/expr_kind.h>

#include <cstdint>

namespace lispb::codegen {

struct Expr {
    std::uint32_t value{};
    explicit operator bool() const { return value != 0; }
    auto kind() const -> ExprKind { return static_cast<ExprKind>(value >> 24); }
    auto index() const -> std::uint32_t { return value & 0x00ffffffU; }
    auto operator==(Expr const&) const -> bool = default;
};

static_assert(sizeof(Expr) == 4);

} // namespace lispb::codegen
