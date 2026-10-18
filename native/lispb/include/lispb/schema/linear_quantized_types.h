#pragma once

#include <codegen/schema/quantization_clipping.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <vector>

namespace lispb::schema {

struct LinearQuantizedTypes {
    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;

    std::vector<ResolvedOccurrenceId> source;
    std::vector<std::uint32_t> bit_width;
    std::vector<std::uint64_t> reserved_codes;
    std::vector<codegen::QuantizationClipping> clipping;
};

} // namespace lispb::schema
