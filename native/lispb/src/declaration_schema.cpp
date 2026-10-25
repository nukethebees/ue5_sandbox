#include <codegen/schema/declaration_schema.h>

#include <ioj/ascii.h>
#include <ioj/is_any.h>

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
        auto value{column[current]};
        if (!transform(value)) {
            return;
        }
        changed = true;
        if constexpr (!std::is_const_v<Columns>) {
            if (current == id) {
                current = columns.clone(id);
            }
            column[current] = value;
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
    auto role = [](TypeReferenceCategory category, StringId subject = {}, std::uint32_t index = 0) {
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
                    kind = (columns.body_lines[row].empty() && !columns.append_columns[row]) ||
                                   columns.definition_in_source[row]
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
    auto source = [&](auto& columns) {
        rewrite_row(columns, declaration, [&](auto field, auto) {
            field(columns.source, [&](auto& value) {
                return reference(role(TypeReferenceCategory::source), value);
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
            source(store.integer_varints);
            break;
        case DeclarationKind::linear_quantized:
            source(store.linear_quantizeds);
            break;
        case DeclarationKind::mini_float:
            break;
        case DeclarationKind::optional_presence_bit:
            source(store.optional_presence_bits);
            break;
        case DeclarationKind::optional_sentinel:
            source(store.optional_sentinels);
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
    }
    return declaration;
}

auto soa_generated_cpp_names(Declarations const& declarations, DeclarationHandle schema)
    -> std::vector<StringId> {
    auto const& soas{declarations.soas};
    auto const row{soas.index(schema)};
    auto& strings{declarations.strings()};
    auto const scope{soa_namespace_name(strings.view(soas.name[row]))};
    std::vector<StringId> result{strings.intern(scope)};
    for (auto const name : {"Schema",
                            "Layout",
                            "ViewImpl",
                            "View",
                            "ConstView",
                            "ResolvedViewImpl",
                            "DataPointers",
                            "ResolvedView",
                            "ResolvedConstView"}) {
        result.push_back(strings.intern(scope + "::" + name));
    }
    if (!soas.layout_only[row]) {
        result.push_back(soas.name[row]);
    }
    if (soas.field_mask_name[row]) {
        result.push_back(*soas.field_mask_name[row]);
    }
    if (soas.field_enum_name[row]) {
        result.push_back(*soas.field_enum_name[row]);
    }
    return result;
}

} // namespace

auto soa_namespace_name(std::string_view const name) -> std::string {
    std::string result;
    auto const count{name.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const character{name[index]};
        auto const previous_lower{index > 0 && ioj::islower(name[index - 1])};
        auto const next_lower{index + 1 < count && ioj::islower(name[index + 1])};
        if (ioj::isupper(character) && !result.empty() && result.back() != '_' &&
            (previous_lower || next_lower)) {
            result += '_';
        }
        result += ioj::tolower(character);
    }
    return result;
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

auto declaration_name(Declarations const& declarations, DeclarationHandle declaration) -> StringId {
    auto name = [declaration](auto const& columns) {
        return columns.name[columns.index(declaration)];
    };
    switch (declaration.kind()) {
        case DeclarationKind::enumeration:
            return name(declarations.enums);
        case DeclarationKind::facade:
            return name(declarations.facades);
        case DeclarationKind::fixed_point:
            return name(declarations.fixed_points);
        case DeclarationKind::integer_scalar:
            return name(declarations.integer_scalars);
        case DeclarationKind::integer_varint:
            return name(declarations.integer_varints);
        case DeclarationKind::linear_quantized:
            return name(declarations.linear_quantizeds);
        case DeclarationKind::mini_float:
            return name(declarations.mini_floats);
        case DeclarationKind::optional_presence_bit:
            return name(declarations.optional_presence_bits);
        case DeclarationKind::optional_sentinel:
            return name(declarations.optional_sentinels);
        case DeclarationKind::packed_value:
            return name(declarations.packed_values);
        case DeclarationKind::record:
            return name(declarations.records);
        case DeclarationKind::soa:
            return name(declarations.soas);
        case DeclarationKind::static_table:
            return name(declarations.static_tables);
        case DeclarationKind::tagged_union:
            return name(declarations.tagged_unions);
        case DeclarationKind::union_type:
            return name(declarations.unions);
    }
    std::unreachable();
}

auto generated_cpp_names(Declarations const& declarations, DeclarationHandle declaration)
    -> std::vector<StringId> {
    auto& strings{declarations.strings()};
    auto text = [&](auto id) { return std::string{strings.view(id)}; };
    auto const id{declaration_name(declarations, declaration)};
    auto const name{text(id)};
    switch (declaration.kind()) {
        case DeclarationKind::enumeration:
            return {id, strings.intern(name + "Underlying")};
        case DeclarationKind::record:
            if (declarations.records.constraint_policy[declaration]) {
                return {id, strings.intern(name + "Value")};
            }
            return {id};
        case DeclarationKind::packed_value:
        case DeclarationKind::union_type:
        case DeclarationKind::tagged_union:
        case DeclarationKind::static_table:
        case DeclarationKind::facade:
            return {id};
        case DeclarationKind::integer_scalar: {
            auto const& scalars{declarations.integer_scalars};
            auto const row{scalars.index(declaration)};
            std::vector<StringId> result;
            if (scalars.cpp_emission[row] == IntegerScalarCppEmission::alias) {
                return {id};
            }
            if (scalars.cpp_emission[row] != IntegerScalarCppEmission::none) {
                for (auto const code : declarations.items(scalars.named_codes[row])) {
                    result.push_back(strings.intern(
                        name + "_" + text(declarations.packed_named_codes.name[code])));
                }
                if (scalars.cpp_emission[row] == IntegerScalarCppEmission::constants_with_names) {
                    result.push_back(strings.intern(name + "_name"));
                }
            }
            return result;
        }
        case DeclarationKind::soa:
            return soa_generated_cpp_names(declarations, declaration);
        default:
            return {};
    }
}

}
