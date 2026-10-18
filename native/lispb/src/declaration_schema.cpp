#include <codegen/schema/declaration_schema.h>

#include <codegen/schema/declarations.h>
#include <codegen/schema/integer_scalar_cpp_emission.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/type_reference_part.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace lispb::codegen {
namespace {

// Publish a new row only when a referenced handle changes.
template <typename Columns, typename Id, typename Rewrite>
auto rewrite_row(Columns& columns, Id& id, Rewrite const& rewrite) -> bool {
    auto current{id};
    bool changed{};
    auto field = [&](auto& column, auto const& transform) {
        auto value{column[columns.index(current)]};
        if (!transform(value)) {
            return;
        }
        changed = true;
        if constexpr (!std::is_const_v<Columns>) {
            if (current == id) {
                current = columns.clone(id);
            }
            column[columns.index(current)] = value;
        }
    };
    rewrite(field, columns.index(id));
    id = current;
    return changed;
}

template <typename Store, typename Id, typename Rewrite>
auto rewrite_range(Store& store, SchemaRange<Id>& range, Rewrite const& rewrite) -> bool {
    std::vector<Id> updated;
    auto const count{range.count};
    for (std::uint32_t index{}; index < count; ++index) {
        auto id{store.items(range)[index]};
        if (rewrite(id, index)) {
            if (updated.empty()) {
                auto const original{store.items(range)};
                updated.assign(original.begin(), original.begin() + index);
            }
        } else if (updated.empty()) {
            continue;
        }
        updated.push_back(id);
    }
    if (updated.empty()) {
        return false;
    }
    if constexpr (!std::is_const_v<Store>) {
        range = store.range(updated);
    }
    return true;
}

template <typename Store, typename Visitor>
auto traverse_references(Store& store, DeclarationHandle declaration, Visitor const& visitor)
    -> DeclarationHandle {
    auto role =
        [](TypeReferenceCategory category, IdentifierId subject = {}, std::uint32_t index = 0) {
            return TypeReferenceRole{.category = category, .subject = subject, .index = index};
        };
    auto reference = [&](TypeReferenceRole location,
                         TypeRefId& id,
                         TypeReferenceKind kind = TypeReferenceKind::ordinary) {
        auto const updated{visitor(location, id, kind)};
        auto const changed{updated != id};
        id = updated;
        return changed;
    };
    auto optional = [&](TypeReferenceRole location,
                        std::optional<TypeRefId>& id,
                        TypeReferenceKind kind = TypeReferenceKind::ordinary) {
        return id && reference(location, *id, kind);
    };
    auto relationship = [&](TypeReferenceRole location,
                            std::optional<SemanticRelationId>& relation) {
        if (!relation) {
            return false;
        }
        location.part = TypeReferencePart::relationship;
        return rewrite_row(store.semantic_relations, *relation, [&](auto field, auto) {
            field(store.semantic_relations.target,
                  [&](auto& value) { return reference(location, value); });
        });
    };
    auto functions = [&](auto& columns, TypeReferenceCategory category, auto& methods) {
        return rewrite_range(store, methods, [&](auto& method, std::uint32_t index) {
            return rewrite_row(columns, method, [&](auto field, auto row) {
                auto location{role(category, columns.name[row], index)};
                auto kind{TypeReferenceKind::ordinary};
                if constexpr (requires { columns.body_lines; }) {
                    kind = columns.body_lines[row].empty() || columns.definition_in_source[row]
                             ? TypeReferenceKind::function_declaration
                             : TypeReferenceKind::function_definition;
                }
                location.part = TypeReferencePart::return_type;
                field(columns.return_type,
                      [&](auto& value) { return reference(location, value, kind); });
                location.part = TypeReferencePart::parameter;
                field(columns.parameters, [&](auto& parameters) {
                    return rewrite_range(
                        store, parameters, [&](auto& parameter, std::uint32_t parameter_index) {
                            return rewrite_row(
                                store.parameters,
                                parameter,
                                [&](auto parameter_field, auto parameter_row) {
                                    location.parameter = store.parameters.name[parameter_row];
                                    location.secondary_index = parameter_index;
                                    auto const parameter_kind{
                                        kind != TypeReferenceKind::ordinary &&
                                                store.parameters.default_value[parameter_row]
                                            ? TypeReferenceKind::complete_definition
                                            : kind};
                                    parameter_field(store.parameters.type, [&](auto& value) {
                                        return reference(location, value, parameter_kind);
                                    });
                                });
                        });
                });
                if constexpr (requires { columns.trailing_return_type; }) {
                    location.part = TypeReferencePart::trailing_return;
                    location.parameter = {};
                    location.secondary_index = {};
                    field(columns.trailing_return_type,
                          [&](auto& value) { return optional(location, value, kind); });
                }
            });
        });
    };
    auto children = [&](auto& columns, auto& values, TypeReferenceCategory category) {
        return rewrite_range(store, values, [&](auto& value, std::uint32_t index) {
            return rewrite_row(columns, value, [&](auto field, auto row) {
                auto const location{role(category, columns.name[row], index)};
                field(columns.type, [&](auto& type) { return reference(location, type); });
                if constexpr (requires { columns.relationship; }) {
                    field(columns.relationship,
                          [&](auto& relation) { return relationship(location, relation); });
                }
            });
        });
    };
    switch (declaration.kind()) {
        case DeclarationKind::enumeration:
            rewrite_row(store.enums, declaration, [&](auto field, auto row) {
                static_cast<void>(row);
                field(store.enums.underlying_type, [&](auto& value) {
                    return optional(role(TypeReferenceCategory::underlying_type), value);
                });
            });
            break;
        case DeclarationKind::facade:
            rewrite_row(store.facades, declaration, [&](auto field, auto row) {
                static_cast<void>(row);
                field(store.facades.target_type, [&](auto& value) {
                    return reference(role(TypeReferenceCategory::target), value);
                });
                field(store.facades.methods, [&](auto& value) {
                    return functions(store.facade_methods, TypeReferenceCategory::method, value);
                });
            });
            break;
        case DeclarationKind::fixed_point:
            break;
        case DeclarationKind::homogeneous_layout:
            rewrite_row(store.homogeneous_layouts, declaration, [&](auto field, auto row) {
                static_cast<void>(row);
                field(store.homogeneous_layouts.value_types, [&](auto& values) {
                    return rewrite_range(store, values, [&](auto& value, std::uint32_t index) {
                        return rewrite_row(
                            store.homogeneous_values, value, [&](auto field, auto row) {
                                auto location{role(
                                    TypeReferenceCategory::layout_value,
                                    store.strings().intern(store.homogeneous_values.suffix[row]),
                                    index)};
                                field(store.homogeneous_values.type,
                                      [&](auto& type) { return reference(location, type); });
                                location.part = TypeReferencePart::equivalent_type;
                                field(store.homogeneous_values.equivalent_type,
                                      [&](auto& type) { return optional(location, type); });
                                location.part = TypeReferencePart::input;
                                field(store.homogeneous_values.input_types, [&](auto& inputs) {
                                    return rewrite_range(
                                        store, inputs, [&](auto& input, std::uint32_t input_index) {
                                            location.secondary_index = input_index;
                                            return reference(location, input);
                                        });
                                });
                            });
                    });
                });
            });
            break;
        case DeclarationKind::integer_scalar:
            rewrite_row(store.integer_scalars, declaration, [&](auto field, auto row) {
                static_cast<void>(row);
                field(store.integer_scalars.cpp_type, [&](auto& value) {
                    return optional(role(TypeReferenceCategory::cpp_type), value);
                });
                field(store.integer_scalars.relationship, [&](auto& value) {
                    return relationship(role(TypeReferenceCategory::value), value);
                });
            });
            break;
        case DeclarationKind::integer_varint:
            rewrite_row(store.integer_varints, declaration, [&](auto field, auto row) {
                static_cast<void>(row);
                field(store.integer_varints.source, [&](auto& value) {
                    return reference(role(TypeReferenceCategory::source), value);
                });
            });
            break;
        case DeclarationKind::linear_quantized:
            rewrite_row(store.linear_quantizeds, declaration, [&](auto field, auto row) {
                static_cast<void>(row);
                field(store.linear_quantizeds.source, [&](auto& value) {
                    return reference(role(TypeReferenceCategory::source), value);
                });
            });
            break;
        case DeclarationKind::mini_float:
            break;
        case DeclarationKind::optional_presence_bit:
            rewrite_row(store.optional_presence_bits, declaration, [&](auto field, auto row) {
                static_cast<void>(row);
                field(store.optional_presence_bits.source, [&](auto& value) {
                    return reference(role(TypeReferenceCategory::source), value);
                });
            });
            break;
        case DeclarationKind::optional_sentinel:
            rewrite_row(store.optional_sentinels, declaration, [&](auto field, auto row) {
                static_cast<void>(row);
                field(store.optional_sentinels.source, [&](auto& value) {
                    return reference(role(TypeReferenceCategory::source), value);
                });
            });
            break;
        case DeclarationKind::packed_value:
            rewrite_row(store.packed_values, declaration, [&](auto field, auto row) {
                static_cast<void>(row);
                field(store.packed_values.storage_type, [&](auto& value) {
                    return reference(role(TypeReferenceCategory::storage), value);
                });
                field(store.packed_values.segments, [&](auto& segments) {
                    return rewrite_range(
                        store, segments, [&](PackedSegmentId& segment, std::uint32_t index) {
                            if (segment.kind() != PackedSegmentKind::field) {
                                return false;
                            }
                            return rewrite_row(
                                store.packed_fields, segment, [&](auto field, auto row) {
                                    auto const location{role(TypeReferenceCategory::field,
                                                             store.packed_fields.name[row],
                                                             index)};
                                    field(store.packed_fields.type,
                                          [&](auto& value) { return reference(location, value); });
                                    field(store.packed_fields.relationship, [&](auto& value) {
                                        return relationship(location, value);
                                    });
                                });
                        });
                });
            });
            break;
        case DeclarationKind::record:
            rewrite_row(store.records, declaration, [&](auto field, auto row) {
                static_cast<void>(row);
                field(store.records.members, [&](auto& value) {
                    return children(store.record_members, value, TypeReferenceCategory::member);
                });
                field(store.records.functions, [&](auto& value) {
                    return functions(store.functions, TypeReferenceCategory::function, value);
                });
            });
            break;
        case DeclarationKind::soa:
            rewrite_row(store.soas, declaration, [&](auto field, auto row) {
                static_cast<void>(row);
                field(store.soas.members, [&](auto& value) {
                    return children(store.soa_members, value, TypeReferenceCategory::member);
                });
                field(store.soas.functions, [&](auto& value) {
                    return functions(store.functions, TypeReferenceCategory::function, value);
                });
                field(store.soas.equivalent_type, [&](auto& value) {
                    return optional(role(TypeReferenceCategory::equivalent_type), value);
                });
                field(store.soas.array_allocator, [&](auto& value) {
                    return optional(role(TypeReferenceCategory::array_allocator), value);
                });
                field(store.soas.const_view_functions, [&](auto& value) {
                    return functions(
                        store.functions, TypeReferenceCategory::const_view_function, value);
                });
                field(store.soas.mutable_view_functions, [&](auto& value) {
                    return functions(store.functions, TypeReferenceCategory::view_function, value);
                });
            });
            break;
        case DeclarationKind::static_table:
            rewrite_row(store.static_tables, declaration, [&](auto field, auto row) {
                static_cast<void>(row);
                field(store.static_tables.columns, [&](auto& value) {
                    return children(
                        store.static_table_columns, value, TypeReferenceCategory::column);
                });
                field(store.static_tables.groups, [&](auto& value) {
                    return children(store.static_table_groups, value, TypeReferenceCategory::group);
                });
            });
            break;
        case DeclarationKind::tagged_union:
            rewrite_row(store.tagged_unions, declaration, [&](auto field, auto row) {
                static_cast<void>(row);
                field(store.tagged_unions.discriminant, [&](auto& value) {
                    return reference(role(TypeReferenceCategory::discriminant), value);
                });
                field(store.tagged_unions.alternatives, [&](auto& value) {
                    return children(
                        store.tagged_union_alternatives, value, TypeReferenceCategory::alternative);
                });
            });
            break;
        case DeclarationKind::union_type:
            rewrite_row(store.unions, declaration, [&](auto field, auto row) {
                static_cast<void>(row);
                field(store.unions.alternatives, [&](auto& value) {
                    return children(
                        store.union_alternatives, value, TypeReferenceCategory::alternative);
                });
            });
            break;
        case DeclarationKind::vector_soa:
            rewrite_row(store.vector_soas, declaration, [&](auto field, auto row) {
                static_cast<void>(row);
                field(store.vector_soas.value_type, [&](auto& value) {
                    return reference(role(TypeReferenceCategory::value_type), value);
                });
                field(store.vector_soas.equivalent_type, [&](auto& value) {
                    return reference(role(TypeReferenceCategory::equivalent_type), value);
                });
            });
            break;
    }
    return declaration;
}

auto soa_generated_cpp_names(Declarations const& declarations,
                             DeclarationHandle schema,
                             ModuleHandle module,
                             bool includes_allocator_variants) -> std::vector<IdentifierId> {
    auto const& soas{declarations.soas};
    auto const schema_row{soas.index(schema)};
    auto const module_row{declarations.normal_modules.index(module)};
    auto& strings{declarations.strings()};
    auto text = [&](auto id) { return std::string{strings.view(id)}; };
    auto const name{text(soas.name[schema_row])};
    auto const view_name{soas.view_name[schema_row] ? text(*soas.view_name[schema_row])
                                                    : name + "View"};
    auto const const_view_name{soas.const_view_name[schema_row]
                                   ? text(*soas.const_view_name[schema_row])
                                   : name + "ConstView"};
    auto const compact_view{!soas.emits_vector_storage(schema) && soas.view_name[schema_row]
                                ? text(*soas.view_name[schema_row])
                                : name + "SingleView"};
    auto const compact_const_view{!soas.emits_vector_storage(schema) &&
                                          soas.const_view_name[schema_row]
                                      ? text(*soas.const_view_name[schema_row])
                                      : name + "SingleConstView"};
    std::vector<IdentifierId> result;
    auto add = [&](std::string_view spelling) { result.push_back(strings.intern(spelling)); };
    add(name + "Schema");
    if (soas.emits_vector_storage(schema)) {
        result.push_back(soas.name[schema_row]);
        add(view_name);
        add(const_view_name);
    }
    if (soas.field_mask_name[schema_row]) {
        result.push_back(*soas.field_mask_name[schema_row]);
    }
    if (soas.field_enum_name[schema_row]) {
        result.push_back(*soas.field_enum_name[schema_row]);
    }
    if (soas.single_allocation[schema_row]) {
        add(text(*soas.single_allocation[schema_row]));
        add(name + "SingleLayout");
        add(name + "SingleLayoutColumns");
        add(compact_view);
        add(compact_const_view);
        add(compact_view + "Impl");
        std::vector<IdentifierId> ancestors{soas.name[schema_row]};
        auto collect_nested =
            [&](auto&& self, DeclarationHandle parent, std::string const& suffix) -> void {
            for (auto const member_id : declarations.items(soas.members[soas.index(parent)])) {
                auto const member{declarations.soa_members.index(member_id)};
                if (declarations.soa_members.kind[member] != SoaMemberKind::nested ||
                    !declarations.soa_members.nested_schema[member]) {
                    continue;
                }
                for (auto const declaration :
                     declarations.items(declarations.normal_modules.declarations[module_row])) {
                    if (declaration.kind() != DeclarationKind::soa) {
                        continue;
                    }
                    auto const nested{soas.index(declaration)};
                    if (soas.name[nested] != *declarations.soa_members.nested_schema[member] ||
                        soas.uses_compact_vector_runtime(declaration) ||
                        std::ranges::find(ancestors, soas.name[nested]) != ancestors.end()) {
                        continue;
                    }
                    auto const path{suffix + "_" + text(declarations.soa_members.name[member])};
                    add(compact_view + path);
                    add(compact_const_view + path);
                    add(compact_view + path + "Impl");
                    ancestors.push_back(soas.name[nested]);
                    self(self, declaration, path);
                    ancestors.pop_back();
                }
            }
        };
        collect_nested(collect_nested, schema, "");
    }
    if (soas.fixed[schema_row]) {
        auto const fixed{declarations.fixed_soas.index(*soas.fixed[schema_row])};
        result.push_back(declarations.fixed_soas.storage_name[fixed]);
        auto const containers{declarations.items(declarations.fixed_soas.containers[fixed])};
        for (auto const container : containers) {
            result.push_back(container);
        }
    }
    if (includes_allocator_variants && soas.emits_vector_storage(schema)) {
        for (auto const id :
             declarations.items(declarations.normal_modules.soa_array_allocators[module_row])) {
            auto const prefix{text(declarations.soa_allocator_variants
                                       .prefix[declarations.soa_allocator_variants.index(id)])};
            add(prefix + name);
            add(prefix + name + "Schema");
            add(prefix + view_name);
            add(prefix + const_view_name);
        }
    }
    return result;
}

} // namespace

auto declaration_head(DeclarationHandle declaration) -> std::string_view {
    switch (declaration.kind()) {
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
    std::unreachable();
}

auto has_primary_semantic_type(DeclarationHandle declaration) -> bool {
    return declaration.kind() != DeclarationKind::homogeneous_layout;
}

void visit_type_references(
    Declarations const& declarations,
    DeclarationHandle declaration,
    std::function<void(TypeReferenceRole const&, TypeRefId, TypeReferenceKind)> const& visit) {
    static_cast<void>(traverse_references(
        declarations, declaration, [&](auto const& role, TypeRefId id, TypeReferenceKind kind) {
            visit(role, id, kind);
            return id;
        }));
}

auto rewrite_type_references(
    Declarations& declarations,
    DeclarationHandle declaration,
    std::function<TypeRefId(TypeReferenceRole const&, TypeRefId, TypeReferenceKind)> const& rewrite)
    -> DeclarationHandle {
    return traverse_references(declarations, declaration, rewrite);
}

auto declaration_name(Declarations const& declarations, DeclarationHandle declaration)
    -> IdentifierId {
    switch (declaration.kind()) {
        case DeclarationKind::enumeration:
            return declarations.enums.name[declarations.enums.index(declaration)];
        case DeclarationKind::facade:
            return declarations.facades.name[declarations.facades.index(declaration)];
        case DeclarationKind::fixed_point:
            return declarations.fixed_points.name[declarations.fixed_points.index(declaration)];
        case DeclarationKind::homogeneous_layout:
            return declarations.homogeneous_layouts
                .name[declarations.homogeneous_layouts.index(declaration)];
        case DeclarationKind::integer_scalar:
            return declarations.integer_scalars
                .name[declarations.integer_scalars.index(declaration)];
        case DeclarationKind::integer_varint:
            return declarations.integer_varints
                .name[declarations.integer_varints.index(declaration)];
        case DeclarationKind::linear_quantized:
            return declarations.linear_quantizeds
                .name[declarations.linear_quantizeds.index(declaration)];
        case DeclarationKind::mini_float:
            return declarations.mini_floats.name[declarations.mini_floats.index(declaration)];
        case DeclarationKind::optional_presence_bit:
            return declarations.optional_presence_bits
                .name[declarations.optional_presence_bits.index(declaration)];
        case DeclarationKind::optional_sentinel:
            return declarations.optional_sentinels
                .name[declarations.optional_sentinels.index(declaration)];
        case DeclarationKind::packed_value:
            return declarations.packed_values.name[declarations.packed_values.index(declaration)];
        case DeclarationKind::record:
            return declarations.records.name[declarations.records.index(declaration)];
        case DeclarationKind::soa:
            return declarations.soas.name[declarations.soas.index(declaration)];
        case DeclarationKind::static_table:
            return declarations.static_tables.name[declarations.static_tables.index(declaration)];
        case DeclarationKind::tagged_union:
            return declarations.tagged_unions.name[declarations.tagged_unions.index(declaration)];
        case DeclarationKind::union_type:
            return declarations.unions.name[declarations.unions.index(declaration)];
        case DeclarationKind::vector_soa:
            return declarations.vector_soas.name[declarations.vector_soas.index(declaration)];
    }
    std::unreachable();
}

auto generated_cpp_names(Declarations const& declarations,
                         DeclarationHandle declaration,
                         ModuleHandle module) -> std::vector<IdentifierId> {
    auto& strings{declarations.strings()};
    auto text = [&](auto id) { return std::string{strings.view(id)}; };
    auto const id{declaration_name(declarations, declaration)};
    auto const name{text(id)};
    switch (declaration.kind()) {
        case DeclarationKind::enumeration:
            return {id, strings.intern(name + "Underlying")};
        case DeclarationKind::packed_value:
        case DeclarationKind::record:
        case DeclarationKind::union_type:
        case DeclarationKind::tagged_union:
        case DeclarationKind::static_table:
        case DeclarationKind::facade:
            return {id};
        case DeclarationKind::integer_scalar: {
            auto const& scalars{declarations.integer_scalars};
            auto const row{scalars.index(declaration)};
            std::vector<IdentifierId> result;
            if (scalars.cpp_emission[row] == IntegerScalarCppEmission::alias) {
                return {id};
            }
            if (scalars.cpp_emission[row] != IntegerScalarCppEmission::none) {
                for (auto const code : declarations.items(scalars.named_codes[row])) {
                    result.push_back(strings.intern(
                        name + "_" +
                        text(declarations.packed_named_codes
                                 .name[declarations.packed_named_codes.index(code)])));
                }
                if (scalars.cpp_emission[row] == IntegerScalarCppEmission::constants_with_names) {
                    result.push_back(strings.intern(name + "_name"));
                }
            }
            return result;
        }
        case DeclarationKind::soa:
            return soa_generated_cpp_names(declarations, declaration, module, true);
        case DeclarationKind::vector_soa: {
            std::vector<IdentifierId> result{strings.intern(name + "Schema"),
                                             id,
                                             strings.intern(name + "View"),
                                             strings.intern(name + "ConstView")};
            auto const fixed{
                declarations.vector_soas.fixed[declarations.vector_soas.index(declaration)]};
            if (fixed) {
                auto const row{declarations.fixed_soas.index(*fixed)};
                result.push_back(declarations.fixed_soas.storage_name[row]);
                for (auto container : declarations.items(declarations.fixed_soas.containers[row])) {
                    result.push_back(container);
                }
            }
            return result;
        }
        case DeclarationKind::homogeneous_layout: {
            std::vector<IdentifierId> result{strings.intern("T" + name + "View")};
            auto const values{declarations.items(
                declarations.homogeneous_layouts
                    .value_types[declarations.homogeneous_layouts.index(declaration)])};
            auto const& homogeneous{declarations.homogeneous_values};
            if (!values.empty() && homogeneous.equivalent_type[homogeneous.index(values.front())]) {
                result.push_back(strings.intern("T" + name + "EquivalentType"));
            }
            for (auto value : values) {
                result.push_back(strings.intern(
                    "F" + name + text(homogeneous.suffix[homogeneous.index(value)])));
            }
            return result;
        }
        default:
            return {};
    }
}

}
