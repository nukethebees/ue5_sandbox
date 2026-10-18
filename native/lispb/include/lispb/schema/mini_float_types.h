#pragma once

#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <vector>

namespace lispb::schema {

struct MiniFloatTypes {
    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;

    std::vector<std::uint32_t> sign_bits;
    std::vector<std::uint32_t> exponent_bits;
    std::vector<std::uint32_t> significand_bits;
    std::vector<std::int32_t> exponent_bias;
};

} // namespace lispb::schema
