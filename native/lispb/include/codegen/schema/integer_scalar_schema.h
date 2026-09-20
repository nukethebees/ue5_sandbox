#pragma once

#include <codegen/schema/packed_field_schema.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct IntegerScalarSchema {
    std::string name;
    bool signedness{};
    PackedIntegerValue minimum_value;
    PackedIntegerValue maximum_value;
    std::optional<std::uint32_t> bit_width;
    std::vector<PackedNamedCodeSchema> named_codes;
};

} // namespace codegen
