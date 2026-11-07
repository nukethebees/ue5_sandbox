#pragma once

#include <codegen/schema/quantization_clipping.h>
#include <lispb/indexed_column.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>

namespace lispb::schema {

struct LinearQuantizedTypes {
    using Index = TaggedIndex<TypeDefinitionId, TypeDefinitionKind::linear_quantized>;

    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;

    IndexedColumn<ResolvedOccurrenceId, Index> source;
    IndexedColumn<std::uint32_t, Index> bit_width;
    IndexedColumn<std::uint64_t, Index> reserved_codes;
    IndexedColumn<codegen::QuantizationClipping, Index> clipping;
};

} // namespace lispb::schema
