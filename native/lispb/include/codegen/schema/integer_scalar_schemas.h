#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/integer_scalar_cpp_emission.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>

namespace lispb::codegen {

struct IntegerScalarSchemas {
    using Indexing = TaggedIndex<DeclarationHandle, DeclarationKind::integer_scalar>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<std::uint8_t> signedness;
    Column<PackedIntegerValue> minimum_value;
    Column<PackedIntegerValue> maximum_value;
    Column<std::optional<std::uint32_t>> bit_width;
    Column<SchemaRange<PackedNamedCodeId>> named_codes;
    Column<std::optional<SemanticRelationId>> relationship;
    Column<IntegerScalarCppEmission> cpp_emission;
    Column<std::optional<TypeRefId>> cpp_type;
};

}
