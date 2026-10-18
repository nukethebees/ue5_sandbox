#pragma once

#include <lispb/schema/type_definition_kind.h>

#include <compare>
#include <cstdint>

namespace lispb::schema {

struct TypeDefinitionId {
    std::uint32_t value{0xffffffffu};
    [[nodiscard]] constexpr auto kind() const -> TypeDefinitionKind {
        return static_cast<TypeDefinitionKind>(value >> 24);
    }
    [[nodiscard]] constexpr auto index() const -> std::uint32_t { return value & 0x00ffffffu; }
    [[nodiscard]] constexpr auto valid() const -> bool { return value != 0xffffffffu; }
    auto operator<=>(TypeDefinitionId const&) const = default;
};
static_assert(sizeof(TypeDefinitionId) == 4);

} // namespace lispb::schema
