#pragma once

#include <codegen/schema/enum_conversion.h>
#include <codegen/schema/enum_reflection.h>
#include <codegen/schema/enum_unreal_projection.h>
#include <codegen/schema/enumerator_schema.h>
#include <codegen/schema/type_ref.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct EnumSchema {
    std::string name;
    std::optional<TypeRef> underlying_type;
    std::optional<std::uint32_t> bit_width;
    std::optional<bool> signedness;
    EnumReflection reflection{EnumReflection::none};
    std::vector<EnumeratorSchema> values;
    bool enum_array{false};
    std::optional<std::string> count;
    std::vector<EnumConversion> conversions;
    std::optional<std::string> export_specifier;
    bool native_api{false};
    std::optional<EnumUnrealProjection> unreal_projection;
};

} // namespace codegen
