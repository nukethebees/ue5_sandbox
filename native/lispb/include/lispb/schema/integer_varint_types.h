#pragma once

#include <codegen/schema/integer_varint_encoding.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <vector>

namespace lispb::schema {

struct IntegerVarintTypes {
    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;

    std::vector<ResolvedOccurrenceId> source;
    std::vector<codegen::IntegerVarintEncoding> encoding;
};

} // namespace lispb::schema
