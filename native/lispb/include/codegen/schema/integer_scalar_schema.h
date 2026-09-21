#pragma once

#include <codegen/schema/packed_field_schema.h>
#include <codegen/schema/semantic_relation_schema.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace codegen {

enum class IntegerScalarCppEmission { none, constants, constants_with_names };

[[nodiscard]] constexpr auto
    integer_scalar_cpp_emission_name(IntegerScalarCppEmission const emission) -> std::string_view {
    switch (emission) {
        case IntegerScalarCppEmission::none:
            return "none";
        case IntegerScalarCppEmission::constants:
            return "constants";
        case IntegerScalarCppEmission::constants_with_names:
            return "constants-with-names";
    }
    return "none";
}

struct IntegerScalarSchema {
    std::string name;
    bool signedness{};
    PackedIntegerValue minimum_value;
    PackedIntegerValue maximum_value;
    std::optional<std::uint32_t> bit_width;
    std::vector<PackedNamedCodeSchema> named_codes;
    std::optional<SemanticRelationSchema> relationship;
    IntegerScalarCppEmission cpp_emission{IntegerScalarCppEmission::none};
    std::optional<TypeRef> cpp_type{};
};

} // namespace codegen
