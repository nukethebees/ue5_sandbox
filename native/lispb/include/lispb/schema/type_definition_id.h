#pragma once

#include <lispb/schema/type_definition_kind.h>

#include <compare>
#include <cstdint>

namespace lispb::schema {

struct TypeDefinitionId {
    static constexpr std::uint32_t index_mask{0x00ffffffu};
    static constexpr std::uint32_t kind_shift{24};

    std::uint32_t value{0xffffffffu};
    [[nodiscard]] constexpr auto kind() const -> TypeDefinitionKind {
        return static_cast<TypeDefinitionKind>(value >> kind_shift);
    }
    [[nodiscard]] constexpr auto index() const -> std::uint32_t { return value & index_mask; }
    [[nodiscard]] constexpr auto valid() const -> bool { return value != 0xffffffffu; }
    auto operator<=>(TypeDefinitionId const&) const = default;
};
static_assert(sizeof(TypeDefinitionId) == 4);

} // namespace lispb::schema
