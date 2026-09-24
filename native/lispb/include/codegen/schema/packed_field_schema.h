#pragma once

#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/semantic_relation_schema.h>
#include <codegen/schema/type_ref.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct PackedNamedCodeSchema {
    std::string name;
    PackedIntegerValue value;
    bool sentinel{};
    auto operator==(PackedNamedCodeSchema const&) const -> bool = default;
};

struct PackedFieldSchema {
    std::string name;
    TypeRef type;
    std::optional<int> bits;
    PackedFieldKind kind{PackedFieldKind::unsigned_integer};
    bool range_helper{false};
    std::optional<PackedIntegerValue> minimum_value;
    std::optional<PackedIntegerValue> maximum_value;
    std::vector<PackedNamedCodeSchema> named_codes;
    std::optional<SemanticRelationSchema> relationship;
    std::optional<PackedIntegerValue> default_value{};
};

} // namespace codegen
