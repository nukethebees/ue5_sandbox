#pragma once

#include <codegen/schema/packed_field_schema.h>
#include <codegen/schema/type_ref.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct PackedValueSchema {
    std::string name;
    TypeRef storage_type;
    std::vector<PackedFieldSchema> fields;
    std::optional<std::uint64_t> invalid_value;
    std::optional<std::string> export_specifier;
};

} // namespace codegen
