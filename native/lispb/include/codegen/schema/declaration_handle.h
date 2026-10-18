#pragma once

#include <codegen/schema/declaration_kind.h>

#include <cassert>
#include <compare>
#include <cstdint>

namespace lispb::codegen {

struct DeclarationHandle {
    std::uint32_t value{};
    [[nodiscard]] auto kind() const -> DeclarationKind {
        return static_cast<DeclarationKind>(value >> 24);
    }
    [[nodiscard]] auto index() const -> std::uint32_t {
        assert((value & 0x00ffffff) != 0);
        return (value & 0x00ffffff) - 1;
    }
    auto operator<=>(DeclarationHandle const&) const = default;
};

} // namespace lispb::codegen
