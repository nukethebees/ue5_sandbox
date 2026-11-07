#pragma once

#include <codegen/ast/indices.h>
#include <codegen/schema/external_scalar_id.h>
#include <codegen/schema/floating_point_format.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/schema_range.h>

#include <cstdint>
#include <vector>

namespace lispb::codegen {

class CppTypes;
class Declarations;

class RegisteredTypes {
  public:
    RegisteredTypes(CppTypes& types, Declarations& declaration_storage)
        : cpp_types{types}
        , declarations{declaration_storage} {}

    auto append(CppTypeId type, ExternalScalarId scalar = {}) -> RegisteredTypeId;
    auto index(RegisteredTypeId id) const -> std::uint32_t;
    auto add_integer(bool signedness,
                     PackedIntegerValue minimum,
                     PackedIntegerValue maximum,
                     std::uint8_t width,
                     SchemaRange<PackedNamedCodeId> codes) -> ExternalScalarId;
    static auto add_floating(FloatingPointFormat format) -> ExternalScalarId;
    // Require integer scalar metadata.
    auto integer_index(ExternalScalarId scalar) const -> std::uint32_t;
    static auto floating(ExternalScalarId scalar) -> FloatingPointFormat;
    void clear();

    CppTypes& cpp_types;
    Declarations& declarations;
    std::vector<CppTypeId> cpp_type;
    std::vector<ExternalScalarId> semantics;
    std::vector<std::uint8_t> integer_signedness;
    std::vector<PackedIntegerValue> integer_minimums;
    std::vector<PackedIntegerValue> integer_maximums;
    std::vector<std::uint8_t> integer_widths;
    std::vector<SchemaRange<PackedNamedCodeId>> integer_codes;
};

}
