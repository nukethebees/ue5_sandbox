#pragma once

#include <lispb/indices.h>
#include <lispb/schema/enum_signedness.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::schema {

struct EnumTypes {
    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;

    std::vector<ResolvedOccurrenceId> underlying_type;
    std::vector<EnumDomainId> domain;
    std::vector<std::optional<std::uint8_t>> bit_width;
    std::vector<EnumSignedness> signedness;
    std::vector<std::optional<StringId>> count;
    std::vector<std::uint8_t> enum_array;
};

} // namespace lispb::schema
