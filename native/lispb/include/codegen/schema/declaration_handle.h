#pragma once

#include <codegen/schema/declaration_kind.h>

#include <cassert>
#include <compare>
#include <cstdint>

namespace lispb::codegen {

struct DeclarationHandle {
    inline static constexpr std::uint32_t index_mask{0x00ffffffu};
    inline static constexpr std::uint32_t kind_shift{24};

    std::uint32_t value{};
    [[nodiscard]] auto kind() const -> DeclarationKind {
        return static_cast<DeclarationKind>(value >> kind_shift);
    }
    [[nodiscard]] auto index() const -> std::uint32_t {
        assert((value & index_mask) != 0);
        return (value & index_mask) - 1;
    }
    auto operator<=>(DeclarationHandle const&) const = default;
};

} // namespace lispb::codegen
