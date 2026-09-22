#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/normal_module_schema.h>

#include <type_traits>

namespace codegen {
namespace {

template <typename>
inline constexpr bool unhandled_declaration_schema{false};

auto soa_generated_cpp_names(SoaSchema const& schema,
                             NormalModuleSchema const& module,
                             bool const includes_allocator_variants) -> std::vector<std::string> {
    std::vector<std::string> result{schema.name,
                                    schema.view_name.value_or(schema.name + "View"),
                                    schema.const_view_name.value_or(schema.name + "ConstView")};
    if (schema.field_mask_name.has_value()) {
        result.push_back(*schema.field_mask_name);
    }
    if (schema.field_enum_name.has_value()) {
        result.push_back(*schema.field_enum_name);
    }
    if (schema.single_allocation.has_value()) {
        result.push_back(*schema.single_allocation);
        result.push_back(schema.name + "SingleLayout");
        result.push_back(schema.name + "SingleView");
        result.push_back(schema.name + "SingleConstView");
        result.push_back(schema.name + "SingleViewImpl");
    }
    for (auto const& variant : schema.single_allocation_variants) {
        result.push_back(variant.name);
    }
    if (schema.fixed.has_value()) {
        result.push_back(schema.fixed->storage_name);
        result.insert(
            result.end(), schema.fixed->containers.begin(), schema.fixed->containers.end());
    }
    if (includes_allocator_variants) {
        for (auto const& allocator : module.soa_array_allocators) {
            result.push_back(allocator.prefix + schema.name);
            result.push_back(allocator.prefix + schema.view_name.value_or(schema.name + "View"));
            result.push_back(allocator.prefix +
                             schema.const_view_name.value_or(schema.name + "ConstView"));
        }
    }
    return result;
}

} // namespace

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
            } else if constexpr (std::is_same_v<T, FacadeSchema>) {
                return DeclarationKind::facade;
            } else {
                static_assert(unhandled_declaration_schema<T>);
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
    return std::visit(
        [](auto const& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, EnumSchema> || std::is_same_v<T, IntegerScalarSchema> ||
                          std::is_same_v<T, LinearQuantizedSchema> ||
                          std::is_same_v<T, IntegerVarintSchema> ||
                          std::is_same_v<T, FixedPointSchema> ||
                          std::is_same_v<T, MiniFloatSchema> ||
                          std::is_same_v<T, OptionalSentinelSchema> ||
                          std::is_same_v<T, OptionalPresenceBitSchema> ||
                          std::is_same_v<T, PackedValueSchema> || std::is_same_v<T, RecordSchema> ||
                          std::is_same_v<T, UnionSchema> || std::is_same_v<T, TaggedUnionSchema> ||
                          std::is_same_v<T, SoaSchema> || std::is_same_v<T, VectorSoaSchema>) {
                return true;
            } else if constexpr (std::is_same_v<T, HomogeneousLayoutSchema> ||
                                 std::is_same_v<T, StaticTableSchema> ||
                                 std::is_same_v<T, FacadeSchema>) {
                return false;
            } else {
                static_assert(unhandled_declaration_schema<T>);
            }
        },
        declaration);
}

auto generated_cpp_names(DeclarationSchema const& declaration, NormalModuleSchema const& module)
    -> std::vector<std::string> {
    return std::visit(
        [&](auto const& value) -> std::vector<std::string> {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, EnumSchema> || std::is_same_v<T, PackedValueSchema> ||
                          std::is_same_v<T, RecordSchema> || std::is_same_v<T, UnionSchema> ||
                          std::is_same_v<T, TaggedUnionSchema> ||
                          std::is_same_v<T, StaticTableSchema> || std::is_same_v<T, FacadeSchema>) {
                return {value.name};
            } else if constexpr (std::is_same_v<T, IntegerScalarSchema>) {
                std::vector<std::string> result;
                if (value.cpp_emission != IntegerScalarCppEmission::none) {
                    for (auto const& code : value.named_codes) {
                        result.push_back(value.name + "_" + code.name);
                    }
                    if (value.cpp_emission == IntegerScalarCppEmission::constants_with_names) {
                        result.push_back(value.name + "_name");
                    }
                }
                return result;
            } else if constexpr (std::is_same_v<T, SoaSchema>) {
                return soa_generated_cpp_names(value, module, true);
            } else if constexpr (std::is_same_v<T, VectorSoaSchema>) {
                SoaSchema as_soa{.name = value.name, .fixed = value.fixed};
                return soa_generated_cpp_names(as_soa, module, false);
            } else if constexpr (std::is_same_v<T, HomogeneousLayoutSchema>) {
                std::vector<std::string> result{"T" + value.name + "View"};
                if (!value.value_types.empty() &&
                    value.value_types.front().equivalent_type.has_value()) {
                    result.push_back("T" + value.name + "EquivalentType");
                }
                for (auto const& item : value.value_types) {
                    result.push_back("F" + value.name + item.suffix);
                }
                return result;
            } else if constexpr (std::is_same_v<T, LinearQuantizedSchema> ||
                                 std::is_same_v<T, IntegerVarintSchema> ||
                                 std::is_same_v<T, FixedPointSchema> ||
                                 std::is_same_v<T, MiniFloatSchema> ||
                                 std::is_same_v<T, OptionalSentinelSchema> ||
                                 std::is_same_v<T, OptionalPresenceBitSchema>) {
                return {};
            } else {
                static_assert(unhandled_declaration_schema<T>);
            }
        },
        declaration);
}

} // namespace codegen
