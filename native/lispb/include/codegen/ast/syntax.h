#pragma once

#include <codegen/ast/expr.h>

#include <cassert>
#include <cstdint>

namespace lispb::codegen {

enum class SyntaxKind : std::uint8_t {
    invalid,
    expression,
    name,
    type_reference,
    template_id,
    member_name,
    scope_name,
    initializer_list,
    designated_initializer,
    authored_fragment,
    macro_invocation,
    binding_list,
    pack_expansion,
    capture,
    reference_capture,
};

// Embed values in syntax without allowing syntax to become a value expression.
struct Syntax {
    constexpr Syntax() = default;
    constexpr Syntax(Expr expression)
        : tag{expression ? SyntaxKind::expression : SyntaxKind::invalid}
        , value{expression.value} {}
    constexpr Syntax(SyntaxKind kind, std::uint32_t index)
        : tag{kind}
        , value{index} {}

    explicit constexpr operator bool() const { return tag != SyntaxKind::invalid; }
    constexpr auto kind() const -> SyntaxKind { return tag; }
    constexpr auto index() const -> std::uint32_t { return value; }
    constexpr auto expression() const -> Expr {
        assert(tag == SyntaxKind::expression);
        return Expr{value};
    }
    auto operator==(Syntax const&) const -> bool = default;

    SyntaxKind tag{};
    std::uint32_t value{};
};

}
