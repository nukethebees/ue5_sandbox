#pragma once

#include <codegen/schema/enum_schema.h>
#include <codegen/schema/facade_schema.h>
#include <codegen/schema/fixed_point_schema.h>
#include <codegen/schema/homogeneous_layout_schema.h>
#include <codegen/schema/integer_scalar_schema.h>
#include <codegen/schema/integer_varint_schema.h>
#include <codegen/schema/linear_quantized_schema.h>
#include <codegen/schema/mini_float_schema.h>
#include <codegen/schema/optional_presence_bit_schema.h>
#include <codegen/schema/optional_sentinel_schema.h>
#include <codegen/schema/packed_value_schema.h>
#include <codegen/schema/record_schema.h>
#include <codegen/schema/soa_schema.h>
#include <codegen/schema/static_table_schema.h>
#include <codegen/schema/tagged_union_schema.h>
#include <codegen/schema/union_schema.h>
#include <codegen/schema/vector_soa_schema.h>

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace codegen {

struct NormalModuleSchema;

using DeclarationSchema = std::variant<EnumSchema,
                                       IntegerScalarSchema,
                                       LinearQuantizedSchema,
                                       IntegerVarintSchema,
                                       FixedPointSchema,
                                       MiniFloatSchema,
                                       OptionalSentinelSchema,
                                       OptionalPresenceBitSchema,
                                       PackedValueSchema,
                                       RecordSchema,
                                       UnionSchema,
                                       TaggedUnionSchema,
                                       SoaSchema,
                                       VectorSoaSchema,
                                       HomogeneousLayoutSchema,
                                       StaticTableSchema,
                                       FacadeSchema>;

enum class DeclarationKind {
    enumeration,
    integer_scalar,
    linear_quantized,
    integer_varint,
    fixed_point,
    mini_float,
    optional_sentinel,
    optional_presence_bit,
    packed_value,
    record,
    union_type,
    tagged_union,
    soa,
    vector_soa,
    homogeneous_layout,
    static_table,
    facade,
};

auto declaration_kind(DeclarationSchema const& declaration) -> DeclarationKind;
auto declaration_head(DeclarationSchema const& declaration) -> std::string_view;
auto declaration_name(DeclarationSchema const& declaration) -> std::string const&;
auto contributes_semantic_type(DeclarationSchema const& declaration) -> bool;
auto generated_cpp_names(DeclarationSchema const& declaration, NormalModuleSchema const& module)
    -> std::vector<std::string>;

template <typename Schema>
auto declaration_if(DeclarationSchema* declaration) -> Schema* {
    return declaration == nullptr ? nullptr : std::get_if<Schema>(declaration);
}

template <typename Schema>
auto declaration_if(DeclarationSchema const* declaration) -> Schema const* {
    return declaration == nullptr ? nullptr : std::get_if<Schema>(declaration);
}

} // namespace codegen
