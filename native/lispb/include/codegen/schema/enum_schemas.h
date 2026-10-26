#pragma once

#include <codegen/schema/cpp_projection.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/enum_conversion.h>
#include <codegen/schema/enum_reflection.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/native_enum_helper.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>
#include <lispb/schema/enum_signedness.h>

#include <cstdint>
#include <optional>

namespace lispb::codegen {

struct EnumSchemas {
    using Indexing = TaggedIndex<DeclarationHandle, DeclarationKind::enumeration>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<std::optional<CppProjection>> cpp_projection;
    Column<std::optional<TypeRefId>> underlying_type;
    Column<SchemaRange<EnumeratorId>> values;
    Column<std::optional<StringId>> count;
    Column<SchemaRange<EnumConversion>> conversions;
    Column<StringId> export_specifier;
    Column<std::optional<EnumUnrealProjectionId>> unreal_projection;
    Column<std::optional<std::uint8_t>> bit_width;
    Column<EnumReflection> reflection;
    Column<schema::EnumSignedness> signedness;
    Column<std::uint8_t> enum_array;
    Column<std::uint8_t> native_api;
    Column<SchemaRange<NativeEnumHelper>> helpers;
};

}
