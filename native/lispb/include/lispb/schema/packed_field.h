#pragma once

#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/packed_integer_value.h>
#include <lispb/schema/packed_named_code.h>
#include <lispb/schema/resolved_type_ref.h>
#include <lispb/schema/semantic_relationship.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace lispb::schema {
struct PackedField {
    std::string name;
    ResolvedTypeRef semantic_type;
    std::uint32_t bit_width{};
    bool bit_width_auto{};
    codegen::PackedFieldKind kind{codegen::PackedFieldKind::unsigned_integer};
    bool range_helper{};
    std::optional<codegen::PackedIntegerValue> minimum_value;
    std::optional<codegen::PackedIntegerValue> maximum_value;
    std::vector<PackedNamedCode> named_codes;
    std::optional<SemanticRelationship> relationship;
    std::optional<codegen::PackedIntegerValue> default_value{};
};
}
