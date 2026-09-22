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

struct DeclarationMetadata {
    DeclarationKind kind;
    std::string_view head;
    bool semantic_type;
};

auto declaration_metadata(DeclarationSchema const& declaration) -> DeclarationMetadata {
    return std::visit(
        [](auto const& value) -> DeclarationMetadata {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, EnumSchema>) {
                return {DeclarationKind::enumeration, "enum", true};
            } else if constexpr (std::is_same_v<T, IntegerScalarSchema>) {
                return {DeclarationKind::integer_scalar, "integer-scalar", true};
            } else if constexpr (std::is_same_v<T, LinearQuantizedSchema>) {
                return {DeclarationKind::linear_quantized, "linear-quantized", true};
            } else if constexpr (std::is_same_v<T, IntegerVarintSchema>) {
                return {DeclarationKind::integer_varint, "integer-varint", true};
            } else if constexpr (std::is_same_v<T, FixedPointSchema>) {
                return {DeclarationKind::fixed_point, "fixed-point", true};
            } else if constexpr (std::is_same_v<T, MiniFloatSchema>) {
                return {DeclarationKind::mini_float, "mini-float", true};
            } else if constexpr (std::is_same_v<T, OptionalSentinelSchema>) {
                return {DeclarationKind::optional_sentinel, "optional-sentinel", true};
            } else if constexpr (std::is_same_v<T, OptionalPresenceBitSchema>) {
                return {DeclarationKind::optional_presence_bit, "optional-presence-bit", true};
            } else if constexpr (std::is_same_v<T, PackedValueSchema>) {
                return {DeclarationKind::packed_value, "packed-value", true};
            } else if constexpr (std::is_same_v<T, RecordSchema>) {
                return {DeclarationKind::record, "record", true};
            } else if constexpr (std::is_same_v<T, UnionSchema>) {
                return {DeclarationKind::union_type, "union", true};
            } else if constexpr (std::is_same_v<T, TaggedUnionSchema>) {
                return {DeclarationKind::tagged_union, "tagged-union", true};
            } else if constexpr (std::is_same_v<T, SoaSchema>) {
                return {DeclarationKind::soa, "struct", true};
            } else if constexpr (std::is_same_v<T, VectorSoaSchema>) {
                return {DeclarationKind::vector_soa, "vector-soa", true};
            } else if constexpr (std::is_same_v<T, HomogeneousLayoutSchema>) {
                return {DeclarationKind::homogeneous_layout, "layout", false};
            } else if constexpr (std::is_same_v<T, StaticTableSchema>) {
                return {DeclarationKind::static_table, "table", false};
            } else if constexpr (std::is_same_v<T, FacadeSchema>) {
                return {DeclarationKind::facade, "facade", false};
            } else {
                static_assert(unhandled_declaration_schema<T>);
            }
        },
        declaration);
}

} // namespace

auto declaration_kind(DeclarationSchema const& declaration) -> DeclarationKind {
    return declaration_metadata(declaration).kind;
}

auto declaration_head(DeclarationSchema const& declaration) -> std::string_view {
    return declaration_metadata(declaration).head;
}

auto contributes_semantic_type(DeclarationSchema const& declaration) -> bool {
    return declaration_metadata(declaration).semantic_type;
}

auto declaration_name(DeclarationSchema const& declaration) -> std::string const& {
    return std::visit([](auto const& value) -> std::string const& { return value.name; },
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
