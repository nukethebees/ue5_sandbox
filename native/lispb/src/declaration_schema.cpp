#include <codegen/schema/declaration_schema.h>

#include <type_traits>

namespace codegen {

auto declaration_kind(DeclarationSchema const& declaration) -> DeclarationKind {
    return std::visit(
        [](auto const& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, EnumSchema>) {
                return DeclarationKind::enumeration;
            } else if constexpr (std::is_same_v<T, IntegerScalarSchema>) {
                return DeclarationKind::integer_scalar;
            } else if constexpr (std::is_same_v<T, LinearQuantizedSchema>) {
                return DeclarationKind::linear_quantized;
            } else if constexpr (std::is_same_v<T, IntegerVarintSchema>) {
                return DeclarationKind::integer_varint;
            } else if constexpr (std::is_same_v<T, FixedPointSchema>) {
                return DeclarationKind::fixed_point;
            } else if constexpr (std::is_same_v<T, MiniFloatSchema>) {
                return DeclarationKind::mini_float;
            } else if constexpr (std::is_same_v<T, OptionalSentinelSchema>) {
                return DeclarationKind::optional_sentinel;
            } else if constexpr (std::is_same_v<T, OptionalPresenceBitSchema>) {
                return DeclarationKind::optional_presence_bit;
            } else if constexpr (std::is_same_v<T, PackedValueSchema>) {
                return DeclarationKind::packed_value;
            } else if constexpr (std::is_same_v<T, RecordSchema>) {
                return DeclarationKind::record;
            } else if constexpr (std::is_same_v<T, UnionSchema>) {
                return DeclarationKind::union_type;
            } else if constexpr (std::is_same_v<T, TaggedUnionSchema>) {
                return DeclarationKind::tagged_union;
            } else if constexpr (std::is_same_v<T, SoaSchema>) {
                return DeclarationKind::soa;
            } else if constexpr (std::is_same_v<T, VectorSoaSchema>) {
                return DeclarationKind::vector_soa;
            } else if constexpr (std::is_same_v<T, HomogeneousLayoutSchema>) {
                return DeclarationKind::homogeneous_layout;
            } else if constexpr (std::is_same_v<T, StaticTableSchema>) {
                return DeclarationKind::static_table;
            } else {
                return DeclarationKind::facade;
            }
        },
        declaration);
}

auto declaration_head(DeclarationSchema const& declaration) -> std::string_view {
    switch (declaration_kind(declaration)) {
        case DeclarationKind::enumeration:
            return "enum";
        case DeclarationKind::integer_scalar:
            return "integer-scalar";
        case DeclarationKind::linear_quantized:
            return "linear-quantized";
        case DeclarationKind::integer_varint:
            return "integer-varint";
        case DeclarationKind::fixed_point:
            return "fixed-point";
        case DeclarationKind::mini_float:
            return "mini-float";
        case DeclarationKind::optional_sentinel:
            return "optional-sentinel";
        case DeclarationKind::optional_presence_bit:
            return "optional-presence-bit";
        case DeclarationKind::packed_value:
            return "packed-value";
        case DeclarationKind::record:
            return "record";
        case DeclarationKind::union_type:
            return "union";
        case DeclarationKind::tagged_union:
            return "tagged-union";
        case DeclarationKind::soa:
            return "struct";
        case DeclarationKind::vector_soa:
            return "vector-soa";
        case DeclarationKind::homogeneous_layout:
            return "layout";
        case DeclarationKind::static_table:
            return "table";
        case DeclarationKind::facade:
            return "facade";
    }
    return {};
}

auto declaration_name(DeclarationSchema const& declaration) -> std::string const& {
    return std::visit([](auto const& value) -> std::string const& { return value.name; },
                      declaration);
}

auto contributes_semantic_type(DeclarationSchema const& declaration) -> bool {
    return declaration_kind(declaration) < DeclarationKind::homogeneous_layout;
}

} // namespace codegen
