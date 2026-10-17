#pragma once

#include <codegen/schema/packed_integer_value.h>
#include <lispb/schema/packed_named_code.h>
#include <lispb/schema/resolved_type_ref.h>
#include <lispb/schema/semantic_relationship.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::schema {
struct IntegerScalarType {
    bool signedness{};
    codegen::PackedIntegerValue minimum_value;
    codegen::PackedIntegerValue maximum_value;
    std::uint32_t bit_width{};
    bool bit_width_auto{};
    std::vector<PackedNamedCode> named_codes;
    std::optional<SemanticRelationship> relationship;
    std::optional<ResolvedTypeRef> cpp_representation{};
};
}
