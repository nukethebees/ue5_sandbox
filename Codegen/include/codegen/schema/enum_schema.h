#pragma once

#include <codegen/schema/enum_conversion.h>
#include <codegen/schema/enum_reflection.h>
#include <codegen/schema/enumerator_schema.h>
#include <codegen/schema/type_ref.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct EnumSchema {
    std::string name;
    TypeRef underlying_type;
    EnumReflection reflection{EnumReflection::none};
    std::vector<EnumeratorSchema> values;
    bool enum_array{false};
    std::vector<EnumConversion> conversions;
    std::optional<std::string> export_specifier;
};

} // namespace codegen
