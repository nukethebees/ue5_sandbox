#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/integer_scalar_cpp_emission.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::codegen {

struct IntegerScalarSchemas {
    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    std::vector<IdentifierId> name;
    std::vector<std::uint8_t> signedness;
    std::vector<PackedIntegerValue> minimum_value;
    std::vector<PackedIntegerValue> maximum_value;
    std::vector<std::optional<std::uint32_t>> bit_width;
    std::vector<SchemaRange<PackedNamedCodeId>> named_codes;
    std::vector<std::optional<SemanticRelationId>> relationship;
    std::vector<IntegerScalarCppEmission> cpp_emission;
    std::vector<std::optional<TypeRefId>> cpp_type;
};

}
