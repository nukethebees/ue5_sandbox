#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/enum_conversion.h>
#include <codegen/schema/enum_reflection.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/native_enum_helper.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>
#include <lispb/schema/enum_signedness.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::codegen {

struct EnumSchemas {
    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    std::vector<StringId> name;
    std::vector<std::optional<TypeRefId>> underlying_type;
    std::vector<SchemaRange<EnumeratorId>> values;
    std::vector<std::optional<StringId>> count;
    std::vector<SchemaRange<EnumConversion>> conversions;
    std::vector<StringId> export_specifier;
    std::vector<std::optional<EnumUnrealProjectionId>> unreal_projection;
    std::vector<std::optional<std::uint8_t>> bit_width;
    std::vector<EnumReflection> reflection;
    std::vector<schema::EnumSignedness> signedness;
    std::vector<std::uint8_t> enum_array;
    std::vector<std::uint8_t> native_api;
    std::vector<SchemaRange<NativeEnumHelper>> helpers;
};

}
