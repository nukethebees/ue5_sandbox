#include <lispb/schema/type_graph.h>

#include "packed_value_internal.h"
#include "validation.h"

#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/record_kind.h>
#include <codegen/complete_dependencies.h>
#include <codegen/schema.h>
#include <codegen/schema/child_occurrences.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/enum_schemas.h>
#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/external_scalar_id.h>
#include <codegen/schema/external_scalar_kind.h>
#include <codegen/schema/facade_method_schemas.h>
#include <codegen/schema/facade_schemas.h>
#include <codegen/schema/fixed_point_schemas.h>
#include <codegen/schema/fixed_point_value.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/integer_scalar_schemas.h>
#include <codegen/schema/integer_varint_schemas.h>
#include <codegen/schema/linear_quantized_schemas.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/mini_float_schemas.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/module_kind.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/optional_presence_bit_schemas.h>
#include <codegen/schema/optional_sentinel_schemas.h>
#include <codegen/schema/packed_bit_order.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/parameter_schemas.h>
#include <codegen/schema/physical_type_form.h>
#include <codegen/schema/physical_type_use.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/registered_types.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/semantic_relation_schemas.h>
#include <codegen/schema/setting_schemas.h>
#include <codegen/schema/settings_module_schemas.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/static_table_column_schemas.h>
#include <codegen/schema/static_table_group_schemas.h>
#include <codegen/schema/static_table_row_schemas.h>
#include <codegen/schema/static_table_schemas.h>
#include <codegen/schema/type_reference_category.h>
#include <codegen/schema/type_reference_kind.h>
#include <codegen/schema/type_reference_part.h>
#include <codegen/schema/type_reference_role.h>
#include <codegen/schema/type_refs.h>
#include <codegen/schema/type_registry.h>
#include <codegen/schema/umbrella_module_schemas.h>
#include <codegen/schema/union_schemas.h>
#include <lispb/indices.h>
#include <lispb/schema/child_occurrences.h>
#include <lispb/schema/enum_types.h>
#include <lispb/schema/external_types.h>
#include <lispb/schema/facade_types.h>
#include <lispb/schema/fixed_point_types.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/integer_domain.h>
#include <lispb/schema/integer_scalar_types.h>
#include <lispb/schema/integer_varint_types.h>
#include <lispb/schema/linear_quantized_types.h>
#include <lispb/schema/mini_float_types.h>
#include <lispb/schema/optional_presence_bit_types.h>
#include <lispb/schema/optional_sentinel_types.h>
#include <lispb/schema/record_types.h>
#include <lispb/schema/soa_columns.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/static_table_types.h>
#include <lispb/schema/type_definition_id.h>
#include <lispb/schema/type_definition_kind.h>
#include <lispb/schema/type_definitions.h>
#include <lispb/schema/type_identity.h>
#include <lispb/schema/type_origin.h>
#include <lispb/schema/union_types.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace lispb::schema {

TypeGraph::TypeGraph(codegen::Declarations& declarations, codegen::CppTypes& cpp_types)
    : declarations_{&declarations}
    , cpp_types_{&cpp_types} {}
auto TypeGraph::strings() const -> Strings& {
    return declarations().strings();
}

auto forward_declaration_kind(TypeId const node, TypeGraph const& graph)
    -> std::optional<codegen::RecordKind> {
    if (graph.origin(node) != TypeOrigin::declaration) {
        return std::nullopt;
    }
    switch (graph.definition(node).kind()) {
        case TypeDefinitionKind::union_type:
            return graph.declarations().unions.layout[graph.source_declaration(node)] ==
                           codegen::LayoutKind::packed
                     ? codegen::RecordKind::struct_
                     : codegen::RecordKind::union_;
        case TypeDefinitionKind::facade:
            return codegen::RecordKind::class_;
        case TypeDefinitionKind::soa:
            if (graph.strings().view(graph.cpp_spelling(node)).empty()) {
                return std::nullopt;
            }
            [[fallthrough]];
        case TypeDefinitionKind::record:
        case TypeDefinitionKind::integer_scalar:
        case TypeDefinitionKind::fixed_point:
        case TypeDefinitionKind::linear_quantized:
        case TypeDefinitionKind::mini_float:
        case TypeDefinitionKind::optional_sentinel:
        case TypeDefinitionKind::optional_presence_bit:
        case TypeDefinitionKind::static_table:
            return codegen::RecordKind::struct_;
        default:
            return std::nullopt;
    }
}

auto uses_forward_declaration(ResolvedOccurrenceId const reference,
                              TypeGraph const& graph,
                              codegen::TypeReferenceKind const kind) -> bool {
    auto const& occurrences{graph.occurrences()};
    auto const row{occurrences.index(reference)};
    auto const form{occurrences.physical_form[row]};
    return kind != codegen::TypeReferenceKind::complete_definition &&
           occurrences.names_semantic_type[row] &&
           ((kind == codegen::TypeReferenceKind::function_declaration &&
             form == codegen::PhysicalTypeForm::value) ||
            form == codegen::PhysicalTypeForm::object_pointer ||
            form == codegen::PhysicalTypeForm::lvalue_reference ||
            form == codegen::PhysicalTypeForm::rvalue_reference) &&
           forward_declaration_kind(occurrences.type[row], graph).has_value();
}

auto uses_cross_module_signature_forward_declaration(ResolvedOccurrenceId const reference,
                                                     codegen::TypeReferenceKind const kind,
                                                     TypeGraph const& graph) -> bool {
    return kind == codegen::TypeReferenceKind::function_declaration &&
           graph.cpp_types()
               .dependencies(graph.occurrences().cpp_type[graph.occurrences().index(reference)])
               .empty() &&
           uses_forward_declaration(reference, graph, kind);
}

class TypeGraphBuilder {
  public:
    explicit TypeGraphBuilder(codegen::Manifest const& manifest)
        : manifest_{manifest}
        , store_{manifest.declarations}
        , graph_{store_, manifest.types.cpp_types} {}

    auto build() -> TypeGraph {
        declare_types();
        graph_.enum_domains_ = analyze_enum_domains(manifest_);
        bind_registered_types();
        resolve_type_uses();
        codegen::detail::validate_integer_domains(manifest_, graph_);
        resolve_definitions(true);
        validate_nested_soa_types();
        codegen::detail::validate_manifest(manifest_, graph_);
        resolve_definitions(false);
        validate_aggregate_cycles();
        validate_discriminators();
        graph_.bit_layouts_.reserve(graph_.types().size());
        for (auto const type : graph_.types()) {
            graph_.bit_layouts_.push_back(resolve_bit_layout(graph_, type));
        }
        build_edges();
        codegen::detail::validate_complete_definition_cycles(
            graph_, codegen::detail::complete_dependencies(graph_));
        return std::move(graph_);
    }
  private:
    auto str(StringId const id) const -> std::string {
        return std::string{store_.strings().view(id)};
    }
    auto text(std::string_view const value) -> StringId { return store_.strings().intern(value); }
    auto qualified_name(lispb::codegen::ModuleSettingsId const& module, StringId const name)
        -> StringId {
        return store_.module_settings_store.namespace_name[module]
                 ? store_.strings().intern(
                       str(*store_.module_settings_store.namespace_name[module]) + "::" + str(name))
                 : name;
    }
    auto declaration_name(TypeId const id) const -> std::string { return str(graph_.name(id)); }

    auto add_type(TypeIdentity const identity,
                  StringId const spelling,
                  TypeDefinitionId const definition,
                  codegen::ModuleHandle const module = {},
                  codegen::DeclarationHandle const source = {}) -> TypeId {
        if (graph_.identities_.contains(identity)) {
            throw std::invalid_argument{
                "Duplicate semantic type identity: " + str(identity.module_name) + ":" +
                str(identity.namespace_name) + ":" + str(identity.name)};
        }
        auto const id{TypeId{static_cast<std::uint32_t>(graph_.definitions_ids_.size())}};
        graph_.origins_.push_back(identity.origin);
        graph_.module_names_.push_back(identity.module_name);
        graph_.namespace_names_.push_back(identity.namespace_name);
        graph_.names_.push_back(identity.name);
        graph_.cpp_spellings_.push_back(spelling);
        graph_.definitions_ids_.push_back(definition);
        graph_.modules_.push_back(module);
        graph_.source_declarations_.push_back(source);
        graph_.identities_.emplace(identity, id);
        return id;
    }

    void declare(codegen::ModuleHandle const module,
                 codegen::DeclarationHandle const source,
                 StringId const name,
                 TypeDefinitionId const definition,
                 std::optional<StringId> const physical_name = {}) {
        auto const module_settings{store_.module_settings(module)};
        auto const logical{qualified_name(module_settings, name)};
        auto const physical{physical_name && physical_name->value == 0
                                ? StringId{}
                                : qualified_name(module_settings, physical_name.value_or(name))};
        auto const id{add_type(
            {.origin = TypeOrigin::declaration,
             .module_name = store_.module_settings_store.name[module_settings],
             .namespace_name =
                 store_.module_settings_store.namespace_name[module_settings].value_or(StringId{}),
             .name = name},
            physical,
            definition,
            module,
            source)};
        declarations_.push_back(id);
        graph_.declarations_by_module_name_.emplace(std::pair{module, name}, id);
        if (physical.value) {
            graph_.declarations_by_spelling_.emplace(physical, id);
            graph_.declarations_by_module_name_.emplace(
                std::pair{module, physical_name.value_or(name)}, id);
        }
        if (physical != logical) {
            graph_.declarations_by_spelling_.emplace(logical, id);
        }
        declarations_by_name_.emplace(name, id);
    }

    void declare_types() {
        for (auto const module_id : manifest_.modules) {
            auto const module_settings{store_.module_settings(module_id)};
            if (!modules_by_name_
                     .emplace(store_.module_settings_store.name[module_settings], module_id)
                     .second) {
                throw std::invalid_argument{
                    "Duplicate module name: " +
                    str(store_.module_settings_store.name[module_settings])};
            }
            module_headers_.emplace(
                module_id,
                store_.module_settings_store.header_include[module_settings].value_or(text(
                    std::filesystem::path{str(store_.module_settings_store.header[module_settings])}
                        .filename()
                        .generic_string())));
            if (module_id.kind() != codegen::ModuleKind::normal) {
                continue;
            }
            auto const module{module_id};
            std::set<StringId> names;
            for (auto const source : store_.items(store_.normal_modules.declarations[module])) {
                auto const name{codegen::declaration_name(store_, source)};
                if (!names.insert(name).second) {
                    throw std::invalid_argument{"Duplicate declaration name: " + str(name)};
                }
                auto const kind{[&]() -> TypeDefinitionKind {
                    switch (source.kind()) {
                        case codegen::DeclarationKind::enumeration:
                            return TypeDefinitionKind::enumeration;
                        case codegen::DeclarationKind::integer_scalar:
                            return TypeDefinitionKind::integer_scalar;
                        case codegen::DeclarationKind::linear_quantized:
                            return TypeDefinitionKind::linear_quantized;
                        case codegen::DeclarationKind::integer_varint:
                            return TypeDefinitionKind::integer_varint;
                        case codegen::DeclarationKind::fixed_point:
                            return TypeDefinitionKind::fixed_point;
                        case codegen::DeclarationKind::mini_float:
                            return TypeDefinitionKind::mini_float;
                        case codegen::DeclarationKind::optional_sentinel:
                            return TypeDefinitionKind::optional_sentinel;
                        case codegen::DeclarationKind::optional_presence_bit:
                            return TypeDefinitionKind::optional_presence_bit;
                        case codegen::DeclarationKind::record:
                            return TypeDefinitionKind::record;
                        case codegen::DeclarationKind::union_type:
                            return TypeDefinitionKind::union_type;
                        case codegen::DeclarationKind::static_table:
                            return TypeDefinitionKind::static_table;
                        case codegen::DeclarationKind::facade:
                            return TypeDefinitionKind::facade;
                        case codegen::DeclarationKind::soa:
                            return TypeDefinitionKind::soa;
                        default:
                            std::unreachable();
                    }
                }()};
                auto const definition{TypeDefinitionId{
                    (static_cast<std::uint32_t>(kind) << TypeDefinitionId::kind_shift) |
                    TypeDefinitionId::index_mask}};
                auto const has_physical_name{[&] {
                    switch (source.kind()) {
                        case codegen::DeclarationKind::record:
                            return store_.records.cpp_projection[source].has_value();
                        case codegen::DeclarationKind::union_type:
                            return store_.unions.cpp_projection[source].has_value();
                        case codegen::DeclarationKind::enumeration:
                            return store_.enums.cpp_projection[source].has_value();
                        case codegen::DeclarationKind::integer_scalar:
                            return store_.integer_scalars.cpp_projection[source].has_value();
                        case codegen::DeclarationKind::linear_quantized:
                            return store_.linear_quantizeds.cpp_projection[source].has_value();
                        case codegen::DeclarationKind::fixed_point:
                            return store_.fixed_points.cpp_projection[source].has_value();
                        case codegen::DeclarationKind::mini_float:
                            return store_.mini_floats.cpp_projection[source].has_value();
                        case codegen::DeclarationKind::integer_varint:
                            return store_.integer_varints.cpp_projection[source].has_value();
                        case codegen::DeclarationKind::optional_sentinel:
                            return store_.optional_sentinels.cpp_projection[source].has_value();
                        case codegen::DeclarationKind::optional_presence_bit:
                            return store_.optional_presence_bits.cpp_projection[source].has_value();
                        case codegen::DeclarationKind::soa:
                            return !store_.soas.layout_only[source];
                        default:
                            return true;
                    }
                }()};
                auto const physical{has_physical_name ? std::optional<StringId>{} : StringId{}};
                declare(module_id,
                        source,
                        codegen::declaration_name(store_, source),
                        definition,
                        physical);
            }
        }
    }

    auto external_type(codegen::CppTypeId const cpp_type,
                       codegen::ExternalScalarId const metadata,
                       std::span<StringId const> const names = {}) -> TypeDefinitionId {
        auto& externals{graph_.definitions_.externals};
        auto const result{externals.append()};
        auto const external_index{externals.index(result)};
        externals.cpp_type[external_index] = cpp_type;
        externals.set_registered_names(result, names);
        if (metadata.kind() == codegen::ExternalScalarKind::integer) {
            auto const scalar{manifest_.types.registered.integer_index(metadata)};
            auto const new_integer_scalars_1{graph_.definitions_.integer_scalars.append()};
            graph_.definitions_.integer_scalars
                .signedness[graph_.definitions_.integer_scalars.index(new_integer_scalars_1)] =
                manifest_.types.registered.integer_signedness[scalar];
            graph_.definitions_.integer_scalars
                .minimum_value[graph_.definitions_.integer_scalars.index(new_integer_scalars_1)] =
                manifest_.types.registered.integer_minimums[scalar];
            graph_.definitions_.integer_scalars
                .maximum_value[graph_.definitions_.integer_scalars.index(new_integer_scalars_1)] =
                manifest_.types.registered.integer_maximums[scalar];
            graph_.definitions_.integer_scalars
                .bit_width[graph_.definitions_.integer_scalars.index(new_integer_scalars_1)] =
                manifest_.types.registered.integer_widths[scalar];
            graph_.definitions_.integer_scalars
                .bit_width_auto[graph_.definitions_.integer_scalars.index(new_integer_scalars_1)] =
                false;
            graph_.definitions_.integer_scalars
                .named_codes[graph_.definitions_.integer_scalars.index(new_integer_scalars_1)] =
                manifest_.types.registered.integer_codes[scalar];
            externals.integer_semantics[external_index] = new_integer_scalars_1;
        } else if (metadata.kind() == codegen::ExternalScalarKind::floating) {
            externals.floating_semantics[external_index] =
                manifest_.types.registered.floating(metadata);
        }
        return result;
    }

    void bind_registered_types() {
        auto& strings{store_.strings()};
        for (auto const& [name, registered] : manifest_.types.entries) {
            auto const spelling{strings.intern(manifest_.types.cpp_types.spelling_id(
                manifest_.types.registered
                    .cpp_type[manifest_.types.registered.index(registered)]))};
            auto const spelling_text{str(spelling)};
            auto const separator{spelling_text.rfind("::")};
            auto const declared_name{
                strings.find(separator == std::string::npos
                                 ? std::string_view{spelling_text}
                                 : std::string_view{spelling_text}.substr(separator + 2))};
            std::optional<TypeId> match;
            if (auto const module{modules_by_name_.find(name)}; module != modules_by_name_.end()) {
                match = graph_.find_declared(module->second, declared_name);
            }
            if (!match) {
                auto const [begin, end]{graph_.declarations_by_spelling_.equal_range(spelling)};
                if (begin != end) {
                    if (std::next(begin) != end) {
                        throw std::invalid_argument{
                            "Registered type '@" + str(name) +
                            "' ambiguously matches multiple declarations of '" + spelling_text +
                            "'"};
                    }
                    match = begin->second;
                } else {
                    auto const [local_begin,
                                local_end]{declarations_by_name_.equal_range(spelling)};
                    if (local_begin != local_end && std::next(local_begin) == local_end) {
                        match = local_begin->second;
                    }
                }
            }
            if (match) {
                if (manifest_.types.registered
                        .semantics[manifest_.types.registered.index(registered)]
                        .kind() != codegen::ExternalScalarKind::none) {
                    throw std::invalid_argument{"External scalar registration '@" + str(name) +
                                                "' matches a LispB declaration"};
                }
                graph_.registered_types_.emplace(name, *match);
            } else {
                auto const id{add_type(
                    {.origin = TypeOrigin::registered_external, .name = name},
                    spelling,
                    external_type(manifest_.types.registered
                                      .cpp_type[manifest_.types.registered.index(registered)],
                                  manifest_.types.registered
                                      .semantics[manifest_.types.registered.index(registered)],
                                  std::span{&name, 1}))};
                graph_.registered_types_.emplace(name, id);
            }
        }
    }

    auto raw_external(StringId const name) -> TypeId {
        if (auto const found{raw_external_types_.find(name)}; found != raw_external_types_.end()) {
            return found->second;
        }
        auto const metadata{codegen::external_scalar_schema(manifest_.types, str(name))};
        // Keep occurrence qualifiers and emission dependencies on the resolved use.
        auto const spelling{codegen::native_spelling(str(name))};
        auto const cpp_type{manifest_.types.cpp_types.intern(spelling)};
        auto const id{
            add_type({.origin = TypeOrigin::cpp_spelling, .name = name},
                     text(spelling),
                     external_type(cpp_type, metadata.value_or(codegen::ExternalScalarId{})))};
        raw_external_types_.emplace(name, id);
        return id;
    }

    auto add_occurrence(TypeId const type,
                        codegen::CppTypeId const cpp_type,
                        codegen::PhysicalTypeUse const physical) -> ResolvedOccurrenceId {
        auto& occurrences{graph_.occurrences_};
        auto const id{occurrences.append()};
        auto const row{occurrences.index(id)};
        occurrences.type[row] = type;
        occurrences.cpp_type[row] = cpp_type;
        occurrences.physical_form[row] = physical.form;
        occurrences.object_spelling[row] = physical.object_spelling;
        occurrences.names_semantic_type[row] = physical.names_semantic_type;
        occurrences.cv_qualified[row] = physical.cv_qualified;
        return id;
    }

    auto bind_reference(codegen::TypeRefId const reference_id, codegen::ModuleHandle const module)
        -> ResolvedOccurrenceId {
        auto const key{std::pair{module, reference_id}};
        if (graph_.bindings_.contains(key)) {
            return graph_.resolved(module, reference_id);
        }
        auto const reference{reference_id};
        auto const found{graph_.find_reference(reference_id, module)};
        codegen::ResolvedCppTypeUse resolved;
        TypeId semantic_type;
        if (found) {
            semantic_type = *found;
            auto const spelling{str(graph_.cpp_spelling(*found))};
            auto& types{manifest_.types.cpp_types};
            if (spelling.empty()) {
                resolved.cpp_type = types.intern("");
                auto const plain{!store_.type_refs.nested[reference] &&
                                 str(store_.type_refs.suffix[reference]).empty()};
                resolved.physical.form = plain ? codegen::PhysicalTypeForm::value
                                               : codegen::PhysicalTypeForm::unsupported;
                resolved.physical.names_semantic_type = plain;
            } else {
                auto base{types.reference(types.intern(spelling))};
                auto const authored_name{store_.strings().view(store_.type_refs.name[reference])};
                if (authored_name.starts_with('@') &&
                    manifest_.types.find(authored_name.substr(1))) {
                    base = codegen::resolve_type_use(reference, manifest_.types).cpp_type;
                } else {
                    std::string suffix;
                    if (store_.type_refs.nested[reference]) {
                        suffix += "::" + str(*store_.type_refs.nested[reference]);
                    }
                    suffix += str(store_.type_refs.suffix[reference]);
                    if (!suffix.empty()) {
                        base = types.decorate(base, "", suffix);
                    }
                }
                resolved.cpp_type = base;
                resolved.physical = codegen::classify_physical_type_use(types, base);
                resolved.physical.names_semantic_type =
                    !store_.type_refs.nested[reference].has_value();
            }
        } else {
            if (store_.strings().view(store_.type_refs.name[reference]).starts_with('@')) {
                throw std::invalid_argument{"Unknown logical type reference: " +
                                            str(store_.type_refs.name[reference])};
            }
            resolved = codegen::resolve_type_use(reference, manifest_.types);
            semantic_type = raw_external(store_.type_refs.name[reference]);
        }
        auto const result{add_occurrence(semantic_type, resolved.cpp_type, resolved.physical)};
        graph_.bindings_.emplace(key, result);
        return result;
    }

    auto resolve_ref(codegen::TypeRefId const reference, codegen::ModuleHandle const module) const
        -> ResolvedOccurrenceId {
        return graph_.resolved(module, reference);
    }
    auto local_declaration(codegen::ModuleHandle const module, StringId const name) const
        -> std::optional<TypeId> {
        return graph_.find_declared(module, name);
    }

    auto resolve_cpp_occurrence(ResolvedOccurrenceId const target,
                                codegen::TypeRefId const reference_id,
                                codegen::ModuleHandle const module,
                                codegen::TypeReferenceRole const& location,
                                codegen::TypeReferenceKind const kind,
                                bool const record_signature,
                                bool const preserve_existing) -> ResolvedOccurrenceId {
        auto const node{graph_.occurrences().type[graph_.occurrences().index(target)]};
        if (graph_.origin(node) != TypeOrigin::declaration) {
            return target;
        }
        if (store_.strings().view(graph_.cpp_spelling(node)).empty()) {
            throw std::invalid_argument{
                "Logical layout schema '" + str(graph_.name(node)) + "' has no C++ owner for " +
                codegen::type_reference_role_text(store_.strings(), location)};
        }
        auto const authored_name{store_.strings().view(store_.type_refs.name[reference_id])};
        auto const registered_reference{authored_name.starts_with('@') &&
                                        manifest_.types.find(authored_name.substr(1)).has_value()};
        if (graph_.module(node) == module || registered_reference ||
            (record_signature &&
             uses_cross_module_signature_forward_declaration(target, kind, graph_))) {
            return target;
        }
        auto& types{manifest_.types.cpp_types};
        auto const existing{
            types.dependencies(graph_.occurrences().cpp_type[graph_.occurrences().index(target)])};
        std::vector<codegen::TypeDependencyId> dependencies{existing.begin(), existing.end()};
        auto const dependency{types.dependency(str(graph_.cpp_spelling(node)),
                                               str(module_headers_.at(graph_.module(node))))};
        if (std::ranges::find(dependencies, dependency) == dependencies.end()) {
            dependencies.push_back(dependency);
        }
        auto const row{graph_.occurrences().index(target)};
        auto const cpp_type{
            types.with_dependencies(graph_.occurrences().cpp_type[row], dependencies)};
        if (!preserve_existing) {
            graph_.occurrences_.cpp_type[row] = cpp_type;
            return target;
        }
        if (cpp_type == graph_.occurrences().cpp_type[row]) {
            return target;
        }
        auto& occurrences{graph_.occurrences_};
        auto const occurrence{occurrences.append()};
        auto const new_row{occurrences.index(occurrence)};
        occurrences.type[new_row] = occurrences.type[row];
        occurrences.cpp_type[new_row] = cpp_type;
        occurrences.physical_form[new_row] = occurrences.physical_form[row];
        occurrences.object_spelling[new_row] = occurrences.object_spelling[row];
        occurrences.names_semantic_type[new_row] = occurrences.names_semantic_type[row];
        occurrences.cv_qualified[new_row] = occurrences.cv_qualified[row];
        return occurrence;
    }

    void add_use(codegen::ModuleHandle const module,
                 codegen::DeclarationHandle const declaration,
                 codegen::TypeReferenceRole const& role,
                 ResolvedOccurrenceId const target,
                 codegen::TypeRefId const reference = {},
                 codegen::TypeReferenceKind const kind = codegen::TypeReferenceKind::ordinary) {
        graph_.use_modules_.push_back(module);
        graph_.use_declarations_.push_back(declaration);
        graph_.use_owners_.push_back(
            declaration.value
                ? graph_.find_declared(module, codegen::declaration_name(store_, declaration))
                : std::nullopt);
        graph_.use_references_.push_back(reference);
        graph_.use_roles_.push_back(role);
        graph_.use_targets_.push_back(target);
        graph_.use_kinds_.push_back(kind);
    }

    void resolve_type_uses() {
        for (auto const module_id : manifest_.modules) {
            if (module_id.kind() == codegen::ModuleKind::normal) {
                auto const module{module_id};
                for (auto const declaration :
                     store_.items(store_.normal_modules.declarations[module])) {
                    codegen::visit_type_references(
                        store_,
                        declaration,
                        [&](codegen::TypeReferenceRole const& role,
                            codegen::TypeRefId const reference,
                            codegen::TypeReferenceKind const kind) {
                            auto const preserve_existing{
                                graph_.bindings_.contains({module_id, reference})};
                            auto target{bind_reference(reference, module_id)};
                            auto emitted{role.part != codegen::TypeReferencePart::relationship};
                            if (declaration.kind() == codegen::DeclarationKind::record) {
                                emitted =
                                    emitted &&
                                    store_.records.cpp_projection[declaration].has_value() &&
                                    (store_.records.layout[declaration] !=
                                         codegen::LayoutKind::packed ||
                                     role.category == codegen::TypeReferenceCategory::function);
                            } else if (declaration.kind() == codegen::DeclarationKind::union_type) {
                                emitted = emitted &&
                                          store_.unions.cpp_projection[declaration].has_value() &&
                                          store_.unions.layout[declaration] !=
                                              codegen::LayoutKind::packed;
                            } else if (declaration.kind() ==
                                           codegen::DeclarationKind::integer_scalar ||
                                       declaration.kind() ==
                                           codegen::DeclarationKind::linear_quantized ||
                                       declaration.kind() ==
                                           codegen::DeclarationKind::integer_varint ||
                                       declaration.kind() ==
                                           codegen::DeclarationKind::optional_sentinel ||
                                       declaration.kind() ==
                                           codegen::DeclarationKind::optional_presence_bit) {
                                emitted = false;
                            }
                            if (declaration.kind() == codegen::DeclarationKind::soa &&
                                role.category == codegen::TypeReferenceCategory::member &&
                                role.part == codegen::TypeReferencePart::direct) {
                                auto const soa{declaration};
                                auto const member{
                                    store_.items(store_.soas.members[soa])[role.index]};
                                emitted = store_.soa_members.kind[member] !=
                                          codegen::SoaMemberKind::nested;
                            }
                            if (emitted) {
                                target = resolve_cpp_occurrence(
                                    target,
                                    reference,
                                    module_id,
                                    role,
                                    kind,
                                    declaration.kind() == codegen::DeclarationKind::record,
                                    preserve_existing);
                            }
                            graph_.bindings_.at({module_id, reference}) = target;
                            add_use(module_id, declaration, role, target, reference, kind);
                        });
                    auto registration = [&](codegen::TypeReferenceRole const& role,
                                            StringId const name) {
                        auto const registered{manifest_.types.find(store_.strings().view(name))};
                        if (!registered) {
                            return;
                        }
                        auto const semantic{graph_.find_registered(name)};
                        assert(semantic);
                        auto physical{codegen::classify_physical_type_use(
                            manifest_.types.cpp_types,
                            manifest_.types.registered
                                .cpp_type[manifest_.types.registered.index(*registered)])};
                        physical.names_semantic_type =
                            physical.form == codegen::PhysicalTypeForm::value;
                        add_use(module_id,
                                declaration,
                                role,
                                add_occurrence(
                                    *semantic,
                                    manifest_.types.registered
                                        .cpp_type[manifest_.types.registered.index(*registered)],
                                    physical));
                    };
                    auto functions =
                        [&](codegen::TypeReferenceCategory const category,
                            codegen::SchemaRange<codegen::FunctionSchemaId> const methods) {
                            std::uint32_t index{};
                            for (auto const method_id : store_.items(methods)) {
                                auto const method{method_id};
                                for (auto const name :
                                     store_.items(store_.functions.dependencies[method])) {
                                    registration({.category = category,
                                                  .part = codegen::TypeReferencePart::dependency,
                                                  .subject = store_.functions.name[method],
                                                  .index = index},
                                                 name);
                                }
                                ++index;
                            }
                        };
                    if (auto const facade{
                            ((declaration).kind() == lispb::codegen::DeclarationKind::facade
                                 ? std::optional{declaration}
                                 : std::nullopt)}) {
                        for (auto const name :
                             store_.items(store_.facades.validation_dependencies[*facade])) {
                            registration(
                                {.category = codegen::TypeReferenceCategory::validation_dependency},
                                name);
                        }
                    } else if (auto const record{
                                   ((declaration).kind() == lispb::codegen::DeclarationKind::record
                                        ? std::optional{declaration}
                                        : std::nullopt)}) {
                        functions(codegen::TypeReferenceCategory::function,
                                  store_.records.functions[*record]);
                    } else if (auto const soa{
                                   ((declaration).kind() == lispb::codegen::DeclarationKind::soa
                                        ? std::optional{declaration}
                                        : std::nullopt)}) {
                        functions(codegen::TypeReferenceCategory::function,
                                  store_.soas.functions[*soa]);
                        functions(codegen::TypeReferenceCategory::const_view_function,
                                  store_.soas.const_view_functions[*soa]);
                        functions(codegen::TypeReferenceCategory::view_function,
                                  store_.soas.mutable_view_functions[*soa]);
                    }
                }
            } else if (module_id.kind() == codegen::ModuleKind::settings) {
                auto const module{module_id};
                std::uint32_t index{};
                for (auto const setting_id :
                     store_.items(store_.settings_modules.settings_list[module])) {
                    auto const setting{setting_id};
                    auto const target{
                        bind_reference(store_.settings.value_type[setting], module_id)};
                    add_use(module_id,
                            {},
                            {.category = codegen::TypeReferenceCategory::setting,
                             .subject = store_.settings.name[setting],
                             .index = index++},
                            target,
                            store_.settings.value_type[setting]);
                }
            }
        }
    }

    auto relationship(std::optional<codegen::SemanticRelationId> const source,
                      codegen::ModuleHandle const module,
                      std::string_view const context) -> RelationshipId {
        if (!source) {
            return {};
        }
        auto const relation{*source};
        auto const key{std::pair{module, relation}};
        if (auto const found{relationships_by_source_.find(key)};
            found != relationships_by_source_.end()) {
            return found->second;
        }
        auto const target{resolve_ref(store_.semantic_relations.target[relation], module)};
        if (graph_.origin(graph_.occurrences().type[graph_.occurrences().index(target)]) !=
            TypeOrigin::declaration) {
            throw std::invalid_argument{
                std::string{context} + " relationship target '" +
                str(store_.type_refs.name[store_.semantic_relations.target[relation]]) +
                "' must resolve to a declared semantic type"};
        }
        auto& relationships{graph_.relationships_};
        auto const id{relationships.append()};
        auto const row{relationships.index(id)};
        relationships.kind[row] = store_.semantic_relations.kind[relation];
        relationships.target[row] = target;
        relationships.unit[row] = store_.semantic_relations.unit[relation];
        relationships_by_source_.emplace(key, id);
        return id;
    }

    auto resolve_normal_definition(codegen::ModuleHandle const module_id,
                                   codegen::DeclarationHandle const source) -> TypeDefinitionId {
        auto& definitions{graph_.definitions_};
        auto const& occurrences{graph_.occurrences()};
        auto const& integer_scalars{definitions.integer_scalars};
        switch (source.kind()) {
            case lispb::codegen::DeclarationKind::enumeration: {
                auto& enums{definitions.enums};
                auto const& schemas{store_.enums};

                auto const result{enums.append()};
                auto const row{enums.index(result)};
                enums.domain[row] = graph_.enum_domains_.find(source);
                enums.underlying_type[row] =
                    schemas.underlying_type[source]
                        .transform([&](auto id) { return resolve_ref(id, module_id); })
                        .value_or(ResolvedOccurrenceId{});
                enums.bit_width[row] = schemas.bit_width[source];
                enums.signedness[row] = schemas.signedness[source];
                enums.count[row] = schemas.count[source];
                enums.enum_array[row] = schemas.enum_array[source];
                return result;
            }
            case lispb::codegen::DeclarationKind::integer_scalar: {
                auto const& schemas{store_.integer_scalars};
                auto const& codes{store_.packed_named_codes};

                auto minimum{schemas.minimum_value[source]};
                auto maximum{schemas.maximum_value[source]};
                for (auto const code_id : store_.items(schemas.named_codes[source])) {
                    auto const code{code_id};
                    if (codegen::packed_integer_less(codes.value[code], minimum)) {
                        minimum = codes.value[code];
                    }
                    if (codegen::packed_integer_less(maximum, codes.value[code])) {
                        maximum = codes.value[code];
                    }
                }
                auto& scalars{definitions.integer_scalars};
                auto const result{scalars.append()};
                auto const scalar_index{scalars.index(result)};
                scalars.signedness[scalar_index] = schemas.signedness[source];
                scalars.minimum_value[scalar_index] = schemas.minimum_value[source];
                scalars.maximum_value[scalar_index] = schemas.maximum_value[source];
                scalars.bit_width[scalar_index] =
                    schemas.bit_width[source].value_or(*codegen::minimum_packed_integer_bits(
                        minimum, maximum, schemas.signedness[source]));
                scalars.bit_width_auto[scalar_index] = !schemas.bit_width[source];
                scalars.named_codes[scalar_index] = schemas.named_codes[source];
                scalars.relationship[scalar_index] =
                    relationship(schemas.relationship[source], module_id, "Integer scalar");
                if (schemas.cpp_projection[source] && schemas.cpp_projection[source]->storage) {
                    scalars.cpp_representation[scalar_index] =
                        resolve_ref(*schemas.cpp_projection[source]->storage, module_id);
                }
                return result;
            }
            case lispb::codegen::DeclarationKind::linear_quantized: {
                auto& types{definitions.linear_quantized};
                auto const& schemas{store_.linear_quantizeds};

                auto const scalar{resolve_ref(schemas.source[source], module_id)};
                if (!integer_domain(graph_, occurrences.type[occurrences.index(scalar)])) {
                    throw std::invalid_argument{"Linear quantization '" +
                                                str(schemas.name[source]) +
                                                "' source must have a declared integer domain"};
                }
                auto const result{types.append()};
                auto const row{types.index(result)};
                types.source[row] = scalar;
                types.bit_width[row] = schemas.bit_width[source];
                types.reserved_codes[row] = schemas.reserved_codes[source];
                types.clipping[row] = schemas.clipping[source];
                return result;
            }
            case lispb::codegen::DeclarationKind::integer_varint: {
                auto& types{definitions.integer_varints};
                auto const& schemas{store_.integer_varints};

                auto const scalar{resolve_ref(schemas.source[source], module_id)};
                if (!integer_domain(graph_, occurrences.type[occurrences.index(scalar)])) {
                    throw std::invalid_argument{"Integer varint '" + str(schemas.name[source]) +
                                                "' source must have a declared integer domain"};
                }
                auto const result{types.append()};
                auto const row{types.index(result)};
                types.source[row] = scalar;
                types.encoding[row] = schemas.encoding[source];
                return result;
            }
            case lispb::codegen::DeclarationKind::fixed_point: {
                auto& points{definitions.fixed_points};
                auto const& schemas{store_.fixed_points};

                auto const result{points.append()};
                auto const row{points.index(result)};
                points.signedness[row] = schemas.signedness[source];
                points.total_bits[row] = schemas.total_bits[source];
                points.fractional_bits[row] = schemas.fractional_bits[source];
                points.rounding[row] = schemas.rounding[source];
                points.minimum_raw_value[row] =
                    schemas.minimum_value[source].transform([&](auto id) {
                        return *codegen::parse_fixed_point_value(store_.strings().view(id),
                                                                 schemas.fractional_bits[source]);
                    });
                points.maximum_raw_value[row] =
                    schemas.maximum_value[source].transform([&](auto id) {
                        return *codegen::parse_fixed_point_value(store_.strings().view(id),
                                                                 schemas.fractional_bits[source]);
                    });
                return result;
            }
            case lispb::codegen::DeclarationKind::mini_float: {
                auto& floats{definitions.mini_floats};
                auto const& schemas{store_.mini_floats};

                auto const result{floats.append()};
                auto const row{floats.index(result)};
                floats.sign_bits[row] = schemas.sign_bits[source];
                floats.exponent_bits[row] = schemas.exponent_bits[source];
                floats.significand_bits[row] = schemas.significand_bits[source];
                floats.exponent_bias[row] = schemas.exponent_bias[source];
                return result;
            }
            case lispb::codegen::DeclarationKind::optional_sentinel: {
                auto& types{definitions.optional_sentinels};
                auto const& schemas{store_.optional_sentinels};

                auto const resolved{resolve_ref(schemas.source[source], module_id)};
                auto const scalar{
                    integer_domain(graph_, occurrences.type[occurrences.index(resolved)])};
                if (!scalar) {
                    throw std::invalid_argument{"Optional sentinel representation '" +
                                                str(schemas.name[source]) +
                                                "' source must have a declared integer domain"};
                }
                auto const codes{
                    store_.items(integer_scalars.named_codes[integer_scalars.index(*scalar)])};
                auto const sentinel{std::ranges::find_if(codes, [&](auto id) {
                    return store_.packed_named_codes.name[id] == schemas.sentinel[source];
                })};
                if (sentinel == codes.end() || !store_.packed_named_codes.sentinel[*sentinel]) {
                    throw std::invalid_argument{"Optional sentinel representation '" +
                                                str(schemas.name[source]) +
                                                "' must name a source sentinel code"};
                }
                auto const result{types.append()};
                auto const row{types.index(result)};
                types.source[row] = resolved;
                types.sentinel_name[row] = schemas.sentinel[source];
                types.sentinel_value[row] = store_.packed_named_codes.value[*sentinel];
                types.bit_width[row] = integer_scalars.bit_width[integer_scalars.index(*scalar)];
                return result;
            }
            case lispb::codegen::DeclarationKind::optional_presence_bit: {
                auto& types{definitions.optional_presence_bits};
                auto const& schemas{store_.optional_presence_bits};

                auto const resolved{resolve_ref(schemas.source[source], module_id)};
                auto const scalar{
                    integer_domain(graph_, occurrences.type[occurrences.index(resolved)])};
                if (!scalar) {
                    throw std::invalid_argument{"Optional presence-bit representation '" +
                                                str(schemas.name[source]) +
                                                "' source must have a declared integer domain"};
                }
                auto const result{types.append()};
                auto const row{types.index(result)};
                types.source[row] = resolved;
                auto const payload_bits{integer_scalars.bit_width[integer_scalars.index(*scalar)]};
                types.payload_bits[row] = payload_bits;
                types.encoded_bits[row] = payload_bits + 1;
                return result;
            }

            case codegen::DeclarationKind::record:
            case codegen::DeclarationKind::union_type: {
                auto const record{source.kind() == codegen::DeclarationKind::record};
                auto const children{record ? store_.records.members[source]
                                           : store_.unions.alternatives[source]};
                std::vector<ChildOccurrenceId> resolved_children;
                auto& values{definitions.children};
                auto const& authored{store_.child_occurrences};
                for (auto const child : store_.items(children)) {
                    auto const id{values.append()};
                    auto const row{values.index(id)};
                    values.source[row] = child;
                    values.name[row] = authored.name[child];
                    values.reserved[row] =
                        authored.kind[child] == codegen::ChildOccurrenceKind::reserved;
                    if (!values.reserved[row]) {
                        values.semantic_type[row] = resolve_ref(authored.type[child], module_id);
                        if (!values.name[row].value) {
                            values.name[row] = graph_.name(
                                occurrences.type[occurrences.index(values.semantic_type[row])]);
                        }
                    }
                    values.count[row] = authored.count[child];
                    values.relationship[row] =
                        relationship(authored.relationship[child], module_id, "Child occurrence");
                    values.initializer[row] = authored.initializer[child];
                    values.bits[row] = authored.bits[child];
                    values.case_label[row] = authored.case_label[child];
                    values.tag[row] = authored.tag[child];
                    values.default_value[row] = authored.default_value[child];
                    resolved_children.push_back(id);
                }
                auto set_layout = [&](auto& types, auto const& schemas, TypeDefinitionId const id) {
                    auto const row{types.index(id)};
                    types.layout[row] = schemas.layout[source];
                    types.bits[row] = schemas.bits[source];
                    auto const projection{schemas.cpp_projection[source]};
                    if (projection && projection->storage) {
                        types.storage_type[row] = resolve_ref(*projection->storage, module_id);
                    }
                };
                if (record) {
                    auto const id{definitions.records.append()};
                    definitions.records.set_members(id, resolved_children);
                    set_layout(definitions.records, store_.records, id);
                    auto const row{definitions.records.index(id)};
                    definitions.records.invalid_raw_value[row] =
                        store_.records.invalid_value[source];
                    definitions.records.bit_order[row] = store_.records.bit_order[source].value_or(
                        codegen::PackedBitOrder::least_significant_first);
                    definitions.records.byte_order[row] = store_.records.byte_order[source];
                    return id;
                }
                auto const id{definitions.unions.append()};
                definitions.unions.set_alternatives(id, resolved_children);
                set_layout(definitions.unions, store_.unions, id);
                return id;
            }
            case lispb::codegen::DeclarationKind::soa: {
                auto& column_definitions{definitions.soa_columns};
                auto const& members{store_.soa_members};
                auto& soas{definitions.soas};

                std::vector<SoaColumnId> columns;
                for (auto const member_id : store_.items(store_.soas.members[source])) {
                    auto const member{member_id};
                    auto const resolved{resolve_ref(members.type[member], module_id)};
                    auto const column{column_definitions.append()};
                    auto const row{column_definitions.index(column)};
                    column_definitions.source[row] = member_id;
                    column_definitions.name[row] = members.name[member];
                    column_definitions.semantic_type[row] = resolved;
                    column_definitions.kind[row] = members.kind[member];
                    column_definitions.relationship[row] =
                        relationship(members.relationship[member],
                                     module_id,
                                     "SOA '" + str(store_.soas.name[source]) + "' member '" +
                                         str(members.name[member]) + "'");
                    columns.push_back(column);
                }
                auto const result{soas.append()};
                auto const row{soas.index(result)};
                soas.set_columns(result, columns);
                soas.equivalent_type[row] =
                    store_.soas.equivalent_type[source]
                        .transform([&](auto id) { return resolve_ref(id, module_id); })
                        .value_or(ResolvedOccurrenceId{});
                soas.layout_only[row] = store_.soas.layout_only[source];
                soas.equivalent_constructor[row] = store_.soas.equivalent_constructor[source];
                return result;
            }
            case lispb::codegen::DeclarationKind::static_table: {
                auto const definition{definitions.static_tables.append()};
                definitions.static_tables.source[definitions.static_tables.index(definition)] =
                    source;
                return definition;
            }
            case lispb::codegen::DeclarationKind::facade: {
                auto const definition{definitions.facades.append()};
                definitions.facades.source[definitions.facades.index(definition)] = source;
                return definition;
            }
            default:
                std::unreachable();
        }
    }

    void resolve_definitions(bool const primary_pass) {
        for (auto const declaration : declarations_) {
            auto const source{graph_.source_declaration(declaration)};
            auto const primary{source.kind() == codegen::DeclarationKind::enumeration ||
                               source.kind() == codegen::DeclarationKind::integer_scalar};
            if (primary != primary_pass) {
                continue;
            }
            graph_.definitions_ids_[declaration.value] =
                resolve_normal_definition(graph_.module(declaration), source);
        }
    }

    void validate_nested_soa_types() const {
        auto const& members{store_.soa_members};
        auto const& occurrences{graph_.occurrences()};
        std::set<TypeId> active;
        std::set<TypeId> visited;
        auto visit = [&](auto&& self, TypeId const type) -> void {
            if (visited.contains(type)) {
                return;
            }
            if (!active.insert(type).second) {
                throw std::invalid_argument{"Cyclic nested SOA: " + str(graph_.name(type))};
            }
            auto const schema{graph_.source_declaration(type)};
            auto const module{graph_.module(type)};
            for (auto const member : store_.items(store_.soas.members[schema])) {
                auto const row{members.index(member)};
                if (members.kind[row] != codegen::SoaMemberKind::nested) {
                    continue;
                }
                auto const occurrence{graph_.resolved(module, members.type[row])};
                auto const use{occurrences.index(occurrence)};
                auto const nested{occurrences.type[use]};
                if (graph_.origin(nested) != TypeOrigin::declaration ||
                    graph_.source_declaration(nested).kind() != codegen::DeclarationKind::soa ||
                    occurrences.physical_form[use] != codegen::PhysicalTypeForm::value ||
                    occurrences.cv_qualified[use]) {
                    throw std::invalid_argument{"Nested SOA member '" + str(members.name[row]) +
                                                "' must reference a declared SOA value type"};
                }
                self(self, nested);
            }
            active.erase(type);
            visited.insert(type);
        };
        for (auto const type : graph_.types()) {
            if (graph_.origin(type) == TypeOrigin::declaration &&
                graph_.source_declaration(type).kind() == codegen::DeclarationKind::soa) {
                visit(visit, type);
            }
        }
    }

    void validate_aggregate_cycle(TypeId const type,
                                  std::vector<std::uint8_t>& states,
                                  std::vector<TypeId>& path) const {
        if (states[type.value] == 2) {
            return;
        }
        if (states[type.value] == 1) {
            std::string message{"Illegal by-value aggregate cycle: "};
            auto const start{std::ranges::find(path, type)};
            for (auto current{start}; current != path.end(); ++current) {
                message += declaration_name(*current) + " -> ";
            }
            throw std::invalid_argument{message + declaration_name(type)};
        }
        states[type.value] = 1;
        path.push_back(type);
        auto const& occurrences{graph_.occurrences()};
        auto const& definitions{graph_.definitions_};
        auto const definition{graph_.definition(type)};
        auto follow = [&](ResolvedOccurrenceId const reference) {
            if (is_valid(reference) && occurrences.contains_value(reference)) {
                validate_aggregate_cycle(
                    occurrences.type[occurrences.index(reference)], states, path);
            }
        };
        switch ((definition).kind()) {
            case lispb::schema::TypeDefinitionKind::record: {
                for (auto const id : definitions.records.members_of(definition)) {
                    follow(definitions.children.semantic_type[definitions.children.index(id)]);
                }
                break;
            }
            case lispb::schema::TypeDefinitionKind::union_type: {
                for (auto const id : definitions.unions.alternatives_of(definition)) {
                    follow(definitions.children.semantic_type[definitions.children.index(id)]);
                }
                break;
            }

            case lispb::schema::TypeDefinitionKind::static_table: {
                auto const source{
                    definitions.static_tables.source[definitions.static_tables.index(definition)]};
                for (auto const column : store_.items(store_.static_tables.columns[source])) {
                    follow(graph_.resolved(graph_.module(type),
                                           store_.static_table_columns.type[column]));
                }
                break;
            }
            default:
                break;
        }
        path.pop_back();
        states[type.value] = 2;
    }

    void validate_aggregate_cycles() const {
        auto const count{graph_.types().size()};
        std::vector<std::uint8_t> states(count);
        std::vector<TypeId> path;
        for (std::uint32_t index{}; index < count; ++index) {
            validate_aggregate_cycle(TypeId{index}, states, path);
        }
    }

    void validate_discriminators() const {
        auto const& definitions{graph_.definitions()};
        auto const& children{definitions.children};
        auto type_of = [&](ChildOccurrenceId const child) {
            auto const occurrence{children.semantic_type[children.index(child)]};
            return graph_.occurrences().type[graph_.occurrences().index(occurrence)];
        };
        for (auto const owner : graph_.types()) {
            auto const definition{graph_.definition(owner)};
            if (definition.kind() != TypeDefinitionKind::record) {
                continue;
            }
            auto const members{definitions.records.members_of(definition)};
            for (auto const member : members) {
                auto const row{children.index(member)};
                if (!children.tag[row]) {
                    continue;
                }
                if (children.reserved[row] || children.count[row]) {
                    throw std::invalid_argument{
                        "A discriminator requires a scalar union occurrence: " +
                        str(children.name[row])};
                }
                auto const union_type{type_of(member)};
                if (graph_.definition(union_type).kind() != TypeDefinitionKind::union_type) {
                    throw std::invalid_argument{"Only a union member can bind a discriminator: " +
                                                str(children.name[row])};
                }
                auto const tag{std::ranges::find_if(members, [&](auto const sibling) {
                    return children.name[children.index(sibling)] == *children.tag[row];
                })};
                if (tag == members.end() || children.reserved[children.index(*tag)] ||
                    children.count[children.index(*tag)] ||
                    graph_.definition(type_of(*tag)).kind() != TypeDefinitionKind::enumeration) {
                    throw std::invalid_argument{
                        "Union discriminator must name a scalar enum sibling: " +
                        str(*children.tag[row])};
                }
                auto const domain{
                    definitions.enums
                        .domain[definitions.enums.index(graph_.definition(type_of(*tag)))]};
                auto const enumerators{graph_.enum_domains().values(domain)};
                std::set<StringId> mappings;
                for (auto const alternative :
                     definitions.unions.alternatives_of(graph_.definition(union_type))) {
                    auto const alternative_row{children.index(alternative)};
                    auto const label{children.case_label[alternative_row].value_or(
                        graph_.name(type_of(alternative)))};
                    auto const value{std::ranges::find_if(enumerators, [&](auto const enumerator) {
                        return store_.enumerators.name[enumerator] == label;
                    })};
                    if (value == enumerators.end() || store_.enumerators.sentinel[*value] ||
                        *value == graph_.enum_domains()
                                      .count_value[graph_.enum_domains().index(domain)] ||
                        !mappings.insert(label).second) {
                        throw std::invalid_argument{
                            "Union case must uniquely map a live discriminator enumerator: " +
                            str(label)};
                    }
                }
            }
        }
    }

    void build_edges() {
        std::vector<std::pair<TypeId, TypeId>> edges;
        auto const count{graph_.types().size()};
        auto const& definitions{graph_.definitions_};
        auto const& occurrences{graph_.occurrences()};
        auto const& relationships{graph_.relationships()};
        for (std::uint32_t index{}; index < count; ++index) {
            auto const owner{TypeId{index}};
            auto const definition{graph_.definitions_ids_[index]};
            auto add = [&](TypeId const dependency) {
                assert(lispb::schema::is_valid(dependency));
                if (dependency != owner) {
                    edges.emplace_back(owner, dependency);
                }
            };
            auto add_occurrence = [&](ResolvedOccurrenceId occurrence) {
                add(occurrences.type[occurrence]);
            };
            auto add_relationship = [&](RelationshipId relationship) {
                if (is_valid(relationship)) {
                    add_occurrence(relationships.target[relationship]);
                }
            };
            switch (definition.kind()) {
                case TypeDefinitionKind::enumeration: {
                    if (auto const underlying{definitions.enums.underlying_type[definition]};
                        is_valid(underlying)) {
                        add_occurrence(underlying);
                    }
                    break;
                }
                case TypeDefinitionKind::static_table: {
                    auto const& tables{definitions.static_tables};
                    auto const source{tables.source[tables.index(definition)]};
                    auto const module{graph_.module(owner)};
                    for (auto const column : store_.items(store_.static_tables.columns[source])) {
                        add_occurrence(
                            graph_.resolved(module, store_.static_table_columns.type[column]));
                    }
                    for (auto const group : store_.items(store_.static_tables.groups[source])) {
                        add_occurrence(
                            graph_.resolved(module, store_.static_table_groups.type[group]));
                    }
                    break;
                }
                case TypeDefinitionKind::facade: {
                    auto const& facades{definitions.facades};
                    auto const source{facades.source[facades.index(definition)]};
                    auto const module{graph_.module(owner)};
                    auto add_reference = [&](codegen::TypeRefId reference) {
                        add_occurrence(graph_.resolved(module, reference));
                    };
                    add_reference(store_.facades.target_type[source]);
                    for (auto const method : store_.items(store_.facades.methods[source])) {
                        add_reference(store_.facade_methods.return_type[method]);
                        for (auto const parameter :
                             store_.items(store_.facade_methods.parameters[method])) {
                            add_reference(store_.parameters.type[parameter]);
                        }
                    }
                    break;
                }
                case TypeDefinitionKind::integer_scalar: {
                    auto const& scalars{definitions.integer_scalars};
                    if (auto const representation{scalars.cpp_representation[definition]};
                        is_valid(representation)) {
                        add_occurrence(representation);
                    }
                    add_relationship(scalars.relationship[definition]);
                    break;
                }
                case TypeDefinitionKind::linear_quantized:
                    add_occurrence(definitions.linear_quantized.source[definition]);
                    break;
                case TypeDefinitionKind::integer_varint:
                    add_occurrence(definitions.integer_varints.source[definition]);
                    break;
                case TypeDefinitionKind::optional_sentinel:
                    add_occurrence(definitions.optional_sentinels.source[definition]);
                    break;
                case TypeDefinitionKind::optional_presence_bit:
                    add_occurrence(definitions.optional_presence_bits.source[definition]);
                    break;
                case TypeDefinitionKind::record: {
                    auto const& members{definitions.children};
                    for (auto const member : definitions.records.members_of(definition)) {
                        if (!members.reserved[member]) {
                            add_occurrence(members.semantic_type[member]);
                        }
                        add_relationship(members.relationship[member]);
                    }
                    break;
                }
                case TypeDefinitionKind::union_type:
                    for (auto const member : definitions.unions.alternatives_of(definition)) {
                        add_occurrence(definitions.children.semantic_type[member]);
                    }
                    break;
                case TypeDefinitionKind::soa: {
                    auto const& columns{definitions.soa_columns};
                    if (auto const equivalent{definitions.soas.equivalent_type[definition]};
                        is_valid(equivalent)) {
                        add_occurrence(equivalent);
                    }
                    for (auto const column : definitions.soas.columns_of(definition)) {
                        add_occurrence(columns.semantic_type[column]);
                        add_relationship(columns.relationship[column]);
                    }
                    break;
                }
                default:
                    break;
            }
        }
        for (auto const use : graph_.type_uses()) {
            auto const owner{graph_.owner(use)};
            if (owner && graph_.role(use).category == codegen::TypeReferenceCategory::function &&
                graph_.definition(*owner).kind() == TypeDefinitionKind::record) {
                auto const target{occurrences.type[graph_.target(use)]};
                if (target != *owner) {
                    edges.emplace_back(*owner, target);
                }
            }
        }
        std::ranges::stable_sort(edges, {}, &std::pair<TypeId, TypeId>::first);
        graph_.dependency_ranges_.resize(count);
        graph_.user_ranges_.resize(count);
        auto edge{edges.begin()};
        for (std::uint32_t index{}; index < count; ++index) {
            auto& range{graph_.dependency_ranges_[index]};
            range.begin = static_cast<std::uint32_t>(graph_.dependencies_.size());
            while (edge != edges.end() && edge->first.value == index) {
                auto const dependency{edge->second};
                auto const current{std::span{graph_.dependencies_}.subspan(range.begin)};
                if (std::ranges::find(current, dependency) == current.end()) {
                    graph_.dependencies_.push_back(dependency);
                    ++range.count;
                    ++graph_.user_ranges_[dependency.value].count;
                }
                ++edge;
            }
        }
        std::uint32_t offset{};
        for (auto& range : graph_.user_ranges_) {
            range.begin = offset;
            offset += range.count;
        }
        graph_.users_.resize(offset);
        std::vector<std::uint32_t> positions(count);
        for (std::uint32_t index{}; index < count; ++index) {
            for (auto const dependency : graph_.dependencies_of(TypeId{index})) {
                auto const range{graph_.user_ranges_[dependency.value]};
                graph_.users_[range.begin + positions[dependency.value]++] = TypeId{index};
            }
        }
    }

    codegen::Manifest const& manifest_;
    codegen::Declarations& store_;
    TypeGraph graph_;
    std::vector<TypeId> declarations_;
    std::map<codegen::ModuleHandle, StringId> module_headers_;
    std::map<StringId, codegen::ModuleHandle> modules_by_name_;
    std::multimap<StringId, TypeId> declarations_by_name_;
    std::map<StringId, TypeId> raw_external_types_;
    std::map<std::pair<codegen::ModuleHandle, codegen::SemanticRelationId>, RelationshipId>
        relationships_by_source_;
};

auto TypeGraph::index(TypeId const id) const -> std::uint32_t {
    assert(lispb::schema::is_valid(id) && id.value < definitions_ids_.size());
    return id.value;
}
auto TypeGraph::index(TypeUseId const id) const -> std::uint32_t {
    assert(id.value < use_modules_.size());
    return id.value;
}
auto TypeGraph::identity(TypeId const id) const -> TypeIdentity {
    auto const row{index(id)};
    return {origins_[row], module_names_[row], namespace_names_[row], names_[row]};
}
auto TypeGraph::origin(TypeId const id) const -> TypeOrigin {
    return origins_[index(id)];
}
auto TypeGraph::module_name(TypeId const id) const -> StringId {
    return module_names_[index(id)];
}
auto TypeGraph::namespace_name(TypeId const id) const -> StringId {
    return namespace_names_[index(id)];
}
auto TypeGraph::name(TypeId const id) const -> StringId {
    return names_[index(id)];
}
auto TypeGraph::cpp_spelling(TypeId const id) const -> StringId {
    return cpp_spellings_[index(id)];
}
auto TypeGraph::definition(TypeId const id) const -> TypeDefinitionId {
    return definitions_ids_[index(id)];
}
auto TypeGraph::module(TypeId const id) const -> codegen::ModuleHandle {
    return modules_[index(id)];
}
auto TypeGraph::source_declaration(TypeId const id) const -> codegen::DeclarationHandle {
    return source_declarations_[index(id)];
}
auto TypeGraph::module(TypeUseId const id) const -> codegen::ModuleHandle {
    return use_modules_[index(id)];
}
auto TypeGraph::declaration(TypeUseId const id) const -> codegen::DeclarationHandle {
    return use_declarations_[index(id)];
}
auto TypeGraph::owner(TypeUseId const id) const -> std::optional<TypeId> {
    return use_owners_[index(id)];
}
auto TypeGraph::reference(TypeUseId const id) const -> codegen::TypeRefId {
    return use_references_[index(id)];
}
auto TypeGraph::role(TypeUseId const id) const -> codegen::TypeReferenceRole {
    return use_roles_[index(id)];
}
auto TypeGraph::kind(TypeUseId const id) const -> codegen::TypeReferenceKind {
    return use_kinds_[index(id)];
}
auto TypeGraph::target(TypeUseId const id) const -> ResolvedOccurrenceId {
    return use_targets_[index(id)];
}

auto TypeGraph::find(TypeIdentity const& identity) const -> std::optional<TypeId> {
    auto const found{identities_.find(identity)};
    return found == identities_.end() ? std::nullopt : std::optional{found->second};
}
auto TypeGraph::find_declared(codegen::ModuleHandle const module, StringId const name) const
    -> std::optional<TypeId> {
    auto const found{declarations_by_module_name_.find({module, name})};
    return found == declarations_by_module_name_.end() ? std::nullopt
                                                       : std::optional{found->second};
}
auto TypeGraph::find_registered(StringId const name) const -> std::optional<TypeId> {
    auto const found{registered_types_.find(name)};
    return found == registered_types_.end() ? std::nullopt : std::optional{found->second};
}
auto TypeGraph::find_reference(codegen::TypeRefId const reference_id,
                               codegen::ModuleHandle const module) const -> std::optional<TypeId> {
    auto const reference{reference_id};
    return find_named(declarations().type_refs.name[declarations().type_refs.index(reference)],
                      module);
}

auto TypeGraph::find_named(StringId const identifier, codegen::ModuleHandle const module) const
    -> std::optional<TypeId> {
    auto const name{strings().view(identifier)};
    if (name.starts_with('@')) {
        auto const logical{name.substr(1)};
        if (auto const separator{logical.find('/')}; separator != std::string_view::npos) {
            auto const module_name{logical.substr(0, separator)};
            auto const type_name{logical.substr(separator + 1)};
            for (auto const type : types()) {
                if (origin(type) == TypeOrigin::declaration &&
                    strings().view(this->module_name(type)) == module_name &&
                    strings().view(this->name(type)) == type_name) {
                    return type;
                }
            }
            return std::nullopt;
        }
        auto const id{strings().find(logical)};
        auto const local{find_declared(module, id)};
        auto const registered{find_registered(id)};
        if (local && registered && local != registered) {
            throw std::invalid_argument{"Ambiguous logical and registered type: " +
                                        std::string{name}};
        }
        return local ? local : registered;
    }
    if (auto const local{find_declared(module, identifier)}) {
        return local;
    }
    auto const [begin, end]{declarations_by_spelling_.equal_range(identifier)};
    return begin != end && std::next(begin) == end ? std::optional{begin->second} : std::nullopt;
}
auto TypeGraph::resolved(codegen::ModuleHandle const module,
                         codegen::TypeRefId const reference) const -> ResolvedOccurrenceId {
    auto const found{bindings_.find({module, reference})};
    assert(found != bindings_.end());
    return found->second;
}
auto TypeGraph::dependencies_of(TypeId const id) const -> std::span<TypeId const> {
    assert(lispb::schema::is_valid(id) && id.value < definitions_ids_.size());
    auto const range{dependency_ranges_[id.value]};
    return std::span{dependencies_}.subspan(range.begin, range.count);
}
auto TypeGraph::users_of(TypeId const id) const -> std::span<TypeId const> {
    assert(lispb::schema::is_valid(id) && id.value < definitions_ids_.size());
    auto const range{user_ranges_[id.value]};
    return std::span{users_}.subspan(range.begin, range.count);
}
auto TypeGraph::types_for_declaration(codegen::DeclarationHandle const declaration) const
    -> std::vector<TypeId> {
    std::vector<TypeId> result;
    auto const count{source_declarations_.size()};
    for (std::uint32_t index{}; index < count; ++index) {
        if (source_declarations_[index] == declaration) {
            result.push_back(TypeId{index});
        }
    }
    return result;
}

auto resolve_type_graph(codegen::Manifest const& manifest) -> TypeGraph {
    return TypeGraphBuilder{manifest}.build();
}

} // namespace lispb::schema
