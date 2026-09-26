#include <codegen/schema/declaration_schema.h>

#include <codegen/schema/normal_module_schema.h>

#include <algorithm>
#include <type_traits>

namespace codegen {
namespace {

template <typename>
inline constexpr bool unhandled_declaration_schema{false};

template <typename Declaration, typename Visitor>
void visit_declaration_type_references(Declaration& declaration, Visitor const& detailed_visit) {
    auto visit = [&](std::string const& role, auto& reference) {
        detailed_visit(role, reference, TypeReferenceKind::ordinary);
    };
    auto optional = [&](std::string const& role,
                        auto& reference,
                        TypeReferenceKind const kind = TypeReferenceKind::ordinary) {
        if (reference.has_value()) {
            detailed_visit(role, *reference, kind);
        }
    };
    auto relationship = [&](std::string const& role, auto& relation) {
        if (relation.has_value()) {
            visit(role + " relationship", relation->target);
        }
    };
    auto function = [&](std::string const& role, auto& method) {
        auto kind{TypeReferenceKind::ordinary};
        if constexpr (std::is_same_v<std::decay_t<decltype(method)>, FunctionSchema>) {
            kind = method.body_lines.empty() || method.definition_in_source
                     ? TypeReferenceKind::function_declaration
                     : TypeReferenceKind::function_definition;
        }
        detailed_visit(role + " return", method.return_type, kind);
        for (auto& parameter : method.parameters) {
            auto const parameter_kind{kind != TypeReferenceKind::ordinary &&
                                              parameter.default_value.has_value()
                                          ? TypeReferenceKind::complete_definition
                                          : kind};
            detailed_visit(role + " parameter " + parameter.name, parameter.type, parameter_kind);
        }
        if constexpr (std::is_same_v<std::decay_t<decltype(method)>, FunctionSchema>) {
            optional(role + " trailing return", method.trailing_return_type, kind);
        }
    };
    std::visit(
        [&](auto& schema) {
            using T = std::decay_t<decltype(schema)>;
            if constexpr (std::is_same_v<T, EnumSchema>) {
                optional("underlying type", schema.underlying_type);
            } else if constexpr (std::is_same_v<T, IntegerScalarSchema>) {
                optional("C++ type", schema.cpp_type);
                relationship("value", schema.relationship);
            } else if constexpr (std::is_same_v<T, LinearQuantizedSchema> ||
                                 std::is_same_v<T, IntegerVarintSchema> ||
                                 std::is_same_v<T, OptionalSentinelSchema> ||
                                 std::is_same_v<T, OptionalPresenceBitSchema>) {
                visit("source", schema.source);
            } else if constexpr (std::is_same_v<T, PackedValueSchema>) {
                visit("storage", schema.storage_type);
                for (auto& segment : schema.segments) {
                    if (auto* field{std::get_if<PackedFieldSchema>(&segment)}) {
                        visit("field " + field->name, field->type);
                        relationship("field " + field->name, field->relationship);
                    }
                }
            } else if constexpr (std::is_same_v<T, RecordSchema> || std::is_same_v<T, SoaSchema>) {
                for (auto& member : schema.members) {
                    visit("member " + member.name, member.type);
                    relationship("member " + member.name, member.relationship);
                }
                if constexpr (std::is_same_v<T, RecordSchema>) {
                    for (std::size_t index{}; index < schema.functions.size(); ++index) {
                        auto& method{schema.functions[index]};
                        auto const role{"function " + method.name + " [" + std::to_string(index) +
                                        "]"};
                        function(role, method);
                    }
                }
                if constexpr (std::is_same_v<T, SoaSchema>) {
                    optional("equivalent type", schema.equivalent_type);
                    optional("array allocator", schema.array_allocator);
                    optional("single allocation allocator", schema.single_allocation_allocator);
                    for (auto& variant : schema.single_allocation_variants) {
                        visit("allocator variant " + variant.name, variant.allocator);
                    }
                    for (std::size_t index{}; index < schema.functions.size(); ++index) {
                        auto& method{schema.functions[index]};
                        auto const role{"function " + method.name + " [" + std::to_string(index) +
                                        "]"};
                        function(role, method);
                    }
                    for (auto& method : schema.const_view_functions) {
                        function("const view function " + method.name, method);
                    }
                    for (std::size_t index{}; index < schema.mutable_view_functions.size();
                         ++index) {
                        auto& method{schema.mutable_view_functions[index]};
                        auto const role{"view function " + method.name + " [" +
                                        std::to_string(index) + "]"};
                        function(role, method);
                    }
                }
            } else if constexpr (std::is_same_v<T, UnionSchema> ||
                                 std::is_same_v<T, TaggedUnionSchema>) {
                if constexpr (std::is_same_v<T, TaggedUnionSchema>) {
                    visit("discriminant", schema.discriminant);
                }
                for (auto& alternative : schema.alternatives) {
                    visit("alternative " + alternative.name, alternative.type);
                }
            } else if constexpr (std::is_same_v<T, VectorSoaSchema>) {
                visit("value type", schema.value_type);
                visit("equivalent type", schema.equivalent_type);
            } else if constexpr (std::is_same_v<T, StaticTableSchema>) {
                for (auto& column : schema.columns) {
                    visit("column " + column.name, column.type);
                }
                for (auto& group : schema.groups) {
                    visit("group " + group.name, group.type);
                }
            } else if constexpr (std::is_same_v<T, FacadeSchema>) {
                visit("target", schema.target_type);
                for (std::size_t index{}; index < schema.methods.size(); ++index) {
                    auto& method{schema.methods[index]};
                    function("method " + method.name + " [" + std::to_string(index) + "]", method);
                }
            } else if constexpr (std::is_same_v<T, HomogeneousLayoutSchema>) {
                for (auto& value : schema.value_types) {
                    auto const role{"value " + value.suffix};
                    visit(role, value.type);
                    optional(role + " equivalent type", value.equivalent_type);
                    for (std::size_t index{}; index < value.input_types.size(); ++index) {
                        visit(role + " input " + std::to_string(index), value.input_types[index]);
                    }
                }
            } else {
                static_assert(std::is_same_v<T, FixedPointSchema> ||
                              std::is_same_v<T, MiniFloatSchema>);
            }
        },
        declaration);
}

auto soa_generated_cpp_names(SoaSchema const& schema,
                             NormalModuleSchema const& module,
                             bool const includes_allocator_variants) -> std::vector<std::string> {
    std::vector<std::string> result;
    if (schema.emits_vector_storage()) {
        result = {schema.name,
                  schema.view_name.value_or(schema.name + "View"),
                  schema.const_view_name.value_or(schema.name + "ConstView")};
    }
    if (schema.field_mask_name.has_value()) {
        result.push_back(*schema.field_mask_name);
    }
    if (schema.field_enum_name.has_value()) {
        result.push_back(*schema.field_enum_name);
    }
    if (schema.single_allocation.has_value()) {
        result.push_back(*schema.single_allocation);
        result.push_back(schema.name + "SingleLayout");
        result.push_back(schema.compact_view_name());
        result.push_back(schema.compact_const_view_name());
        result.push_back(schema.compact_view_name() + "Impl");
        std::vector<std::string> ancestors{schema.name};
        auto collect_nested =
            [&](auto&& self, SoaSchema const& parent, std::string const& suffix) -> void {
            for (auto const& member : parent.members) {
                if (member.kind != SoaMemberKind::nested || !member.nested_schema) {
                    continue;
                }
                for (auto const& declaration : module.declarations) {
                    auto const* nested{std::get_if<SoaSchema>(&declaration)};
                    if (!nested || nested->name != *member.nested_schema ||
                        nested->uses_compact_vector_runtime() ||
                        std::ranges::find(ancestors, nested->name) != ancestors.end()) {
                        continue;
                    }
                    auto const path{suffix + "_" + member.name};
                    result.push_back(schema.compact_view_name() + path);
                    result.push_back(schema.compact_const_view_name() + path);
                    result.push_back(schema.compact_view_name() + path + "Impl");
                    ancestors.push_back(nested->name);
                    self(self, *nested, path);
                    ancestors.pop_back();
                }
            }
        };
        collect_nested(collect_nested, schema, "");
    }
    for (auto const& variant : schema.single_allocation_variants) {
        result.push_back(variant.name);
    }
    if (schema.fixed.has_value()) {
        result.push_back(schema.fixed->storage_name);
        result.insert(
            result.end(), schema.fixed->containers.begin(), schema.fixed->containers.end());
    }
    if (includes_allocator_variants && schema.emits_vector_storage()) {
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
                return {DeclarationKind::static_table, "table", true};
            } else if constexpr (std::is_same_v<T, FacadeSchema>) {
                return {DeclarationKind::facade, "facade", true};
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

auto has_primary_semantic_type(DeclarationSchema const& declaration) -> bool {
    return declaration_metadata(declaration).semantic_type;
}

void visit_type_references(DeclarationSchema const& declaration,
                           std::function<void(std::string const&, TypeRef const&)> const& visit) {
    visit_declaration_type_references(
        declaration, [&](auto const& role, auto const& reference, TypeReferenceKind) {
            visit(role, reference);
        });
}

void visit_type_references(DeclarationSchema& declaration,
                           std::function<void(std::string const&, TypeRef&)> const& visit) {
    visit_declaration_type_references(
        declaration,
        [&](auto const& role, auto& reference, TypeReferenceKind) { visit(role, reference); });
}

void visit_type_references(
    DeclarationSchema const& declaration,
    std::function<void(std::string const&, TypeRef const&, TypeReferenceKind)> const& visit) {
    visit_declaration_type_references(declaration, visit);
}

void visit_type_references(
    DeclarationSchema& declaration,
    std::function<void(std::string const&, TypeRef&, TypeReferenceKind)> const& visit) {
    visit_declaration_type_references(declaration, visit);
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
                if (value.cpp_emission == IntegerScalarCppEmission::alias) {
                    return {value.name};
                }
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
