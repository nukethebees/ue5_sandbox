#include <lispb/schema/type_graph.h>

#include "packed_value_internal.h"
#include "validation.h"

#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/record_kind.h>
#include <codegen/complete_dependencies.h>
#include <codegen/schema.h>
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
#include <codegen/schema/homogeneous_layout_schemas.h>
#include <codegen/schema/homogeneous_value_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/integer_scalar_cpp_emission.h>
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
#include <codegen/schema/packed_field_schemas.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/packed_reserved_bits_schemas.h>
#include <codegen/schema/packed_value_schemas.h>
#include <codegen/schema/parameter_schemas.h>
#include <codegen/schema/physical_type_form.h>
#include <codegen/schema/physical_type_use.h>
#include <codegen/schema/record_member_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/registered_types.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/semantic_relation_schemas.h>
#include <codegen/schema/setting_schemas.h>
#include <codegen/schema/settings_module_schemas.h>
#include <codegen/schema/soa_allocators.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/static_table_column_schemas.h>
#include <codegen/schema/static_table_group_schemas.h>
#include <codegen/schema/static_table_row_schemas.h>
#include <codegen/schema/static_table_schemas.h>
#include <codegen/schema/tagged_union_alternative_schemas.h>
#include <codegen/schema/tagged_union_schemas.h>
#include <codegen/schema/type_reference_category.h>
#include <codegen/schema/type_reference_kind.h>
#include <codegen/schema/type_reference_part.h>
#include <codegen/schema/type_reference_role.h>
#include <codegen/schema/type_refs.h>
#include <codegen/schema/type_registry.h>
#include <codegen/schema/umbrella_module_schemas.h>
#include <codegen/schema/union_alternative_schemas.h>
#include <codegen/schema/union_schemas.h>
#include <codegen/schema/vector_soa_schemas.h>
#include <lispb/indices.h>
#include <lispb/schema/enum_types.h>
#include <lispb/schema/external_types.h>
#include <lispb/schema/facade_types.h>
#include <lispb/schema/fixed_point_types.h>
#include <lispb/schema/homogeneous_storage_types.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/integer_domain.h>
#include <lispb/schema/integer_scalar_types.h>
#include <lispb/schema/integer_varint_types.h>
#include <lispb/schema/linear_quantized_types.h>
#include <lispb/schema/mini_float_types.h>
#include <lispb/schema/optional_presence_bit_types.h>
#include <lispb/schema/optional_sentinel_types.h>
#include <lispb/schema/packed_fields.h>
#include <lispb/schema/packed_reserved_segments.h>
#include <lispb/schema/packed_segment_id.h>
#include <lispb/schema/packed_types.h>
#include <lispb/schema/record_members.h>
#include <lispb/schema/record_types.h>
#include <lispb/schema/soa_columns.h>
#include <lispb/schema/soa_source_kind.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/static_table_types.h>
#include <lispb/schema/tagged_union_alternatives.h>
#include <lispb/schema/tagged_union_types.h>
#include <lispb/schema/type_definition_id.h>
#include <lispb/schema/type_definition_kind.h>
#include <lispb/schema/type_definitions.h>
#include <lispb/schema/type_identity.h>
#include <lispb/schema/type_origin.h>
#include <lispb/schema/union_alternatives.h>
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
            return codegen::RecordKind::union_;
        case TypeDefinitionKind::facade:
            return codegen::RecordKind::class_;
        case TypeDefinitionKind::soa:
            if (graph.strings().view(graph.cpp_spelling(node)).empty()) {
                return std::nullopt;
            }
            [[fallthrough]];
        case TypeDefinitionKind::record:
        case TypeDefinitionKind::tagged_union:
        case TypeDefinitionKind::packed:
        case TypeDefinitionKind::static_table:
        case TypeDefinitionKind::homogeneous_storage:
            return codegen::RecordKind::struct_;
        default:
            return std::nullopt;
    }
}

auto uses_forward_declaration(ResolvedOccurrenceId const reference,
                              TypeGraph const& graph,
                              codegen::TypeReferenceKind const kind) -> bool {
    auto const form{graph.occurrences().physical_form[graph.occurrences().index(reference)]};
    return kind != codegen::TypeReferenceKind::complete_definition &&
           graph.occurrences().names_semantic_type[graph.occurrences().index(reference)] &&
           ((kind == codegen::TypeReferenceKind::function_declaration &&
             form == codegen::PhysicalTypeForm::value) ||
            form == codegen::PhysicalTypeForm::object_pointer ||
            form == codegen::PhysicalTypeForm::lvalue_reference ||
            form == codegen::PhysicalTypeForm::rvalue_reference) &&
           forward_declaration_kind(graph.occurrences().type[graph.occurrences().index(reference)],
                                    graph)
               .has_value();
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
        codegen::detail::validate_manifest(manifest_, graph_);
        resolve_definitions(false);
        validate_nested_soa_types();
        validate_standard_library_soa_columns();
        validate_vector_equivalents();
        validate_aggregate_cycles();
        build_edges();
        codegen::detail::validate_complete_definition_cycles(
            graph_, codegen::detail::complete_dependencies(graph_));
        return std::move(graph_);
    }
  private:
    auto str(IdentifierId const id) const -> std::string {
        return std::string{store_.strings().view(id)};
    }
    auto str(StringId const id) const -> std::string {
        return std::string{store_.strings().view(id)};
    }
    auto text(std::string_view const value) -> StringId {
        return string_id(store_.strings().intern(value));
    }
    auto qualified_name(lispb::codegen::ModuleSettingsId const& module, IdentifierId const name)
        -> IdentifierId {
        return store_.module_settings_store
                       .namespace_name[store_.module_settings_store.index(module)]
                 ? store_.strings().intern(
                       str(*store_.module_settings_store
                                .namespace_name[store_.module_settings_store.index(module)]) +
                       "::" + str(name))
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
                 IdentifierId const name,
                 TypeDefinitionId const definition,
                 std::optional<IdentifierId> const physical_name = {}) {
        auto const module_settings{store_.module_settings(module)};
        auto const logical{qualified_name(module_settings, name)};
        auto const physical{physical_name && physical_name->value == 0
                                ? IdentifierId{}
                                : qualified_name(module_settings, physical_name.value_or(name))};
        auto const id{
            add_type({.origin = TypeOrigin::declaration,
                      .module_name = store_.module_settings_store
                                         .name[store_.module_settings_store.index(module_settings)],
                      .namespace_name =
                          store_.module_settings_store
                              .namespace_name[store_.module_settings_store.index(module_settings)]
                              .value_or(IdentifierId{}),
                      .name = name},
                     string_id(physical),
                     definition,
                     module,
                     source)};
        declarations_.push_back(id);
        homogeneous_values_.emplace_back();
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
                     .emplace(store_.module_settings_store
                                  .name[store_.module_settings_store.index(module_settings)],
                              module_id)
                     .second) {
                throw std::invalid_argument{
                    "Duplicate module name: " +
                    str(store_.module_settings_store
                            .name[store_.module_settings_store.index(module_settings)])};
            }
            module_headers_.emplace(
                module_id,
                store_.module_settings_store
                    .header_include[store_.module_settings_store.index(module_settings)]
                    .value_or(text(std::filesystem::path{
                        str(store_.module_settings_store
                                .header[store_.module_settings_store.index(module_settings)])}
                                       .filename()
                                       .generic_string())));
            if (module_id.kind() != codegen::ModuleKind::normal) {
                continue;
            }
            auto const module{module_id};
            std::set<IdentifierId> names;
            for (auto const source : store_.items(
                     store_.normal_modules.declarations[store_.normal_modules.index(module)])) {
                auto const name{codegen::declaration_name(store_, source)};
                if (!names.insert(name).second) {
                    throw std::invalid_argument{"Duplicate declaration name: " + str(name)};
                }
                if (auto const layout{
                        ((source).kind() == lispb::codegen::DeclarationKind::homogeneous_layout
                             ? std::optional{source}
                             : std::nullopt)}) {
                    for (auto const value_id : store_.items(
                             store_.homogeneous_layouts
                                 .value_types[store_.homogeneous_layouts.index(*layout)])) {
                        auto const value{value_id};
                        declare(module_id,
                                source,
                                store_.strings().intern(
                                    "F" +
                                    str(store_.homogeneous_layouts
                                            .name[store_.homogeneous_layouts.index(*layout)]) +
                                    str(store_.homogeneous_values
                                            .suffix[store_.homogeneous_values.index(value)])),
                                lispb::schema::TypeDefinitionId{
                                    (static_cast<std::uint32_t>(
                                         lispb::schema::TypeDefinitionKind::homogeneous_storage)
                                     << 24) |
                                    0xffffffu});
                        homogeneous_values_.back() = value_id;
                    }
                    continue;
                }
                if (!codegen::has_primary_semantic_type(source)) {
                    continue;
                }
                auto const definition{[&]() -> TypeDefinitionId {
                    auto const value{source};
                    switch (value.kind()) {
                        case lispb::codegen::DeclarationKind::enumeration: {

                            return lispb::schema::TypeDefinitionId{
                                (static_cast<std::uint32_t>(
                                     lispb::schema::TypeDefinitionKind::enumeration)
                                 << 24) |
                                0xffffffu};
                        }
                        case lispb::codegen::DeclarationKind::integer_scalar: {

                            return lispb::schema::TypeDefinitionId{
                                (static_cast<std::uint32_t>(
                                     lispb::schema::TypeDefinitionKind::integer_scalar)
                                 << 24) |
                                0xffffffu};
                        }
                        case lispb::codegen::DeclarationKind::linear_quantized: {

                            return lispb::schema::TypeDefinitionId{
                                (static_cast<std::uint32_t>(
                                     lispb::schema::TypeDefinitionKind::linear_quantized)
                                 << 24) |
                                0xffffffu};
                        }
                        case lispb::codegen::DeclarationKind::integer_varint: {

                            return lispb::schema::TypeDefinitionId{
                                (static_cast<std::uint32_t>(
                                     lispb::schema::TypeDefinitionKind::integer_varint)
                                 << 24) |
                                0xffffffu};
                        }
                        case lispb::codegen::DeclarationKind::fixed_point: {

                            return lispb::schema::TypeDefinitionId{
                                (static_cast<std::uint32_t>(
                                     lispb::schema::TypeDefinitionKind::fixed_point)
                                 << 24) |
                                0xffffffu};
                        }
                        case lispb::codegen::DeclarationKind::mini_float: {

                            return lispb::schema::TypeDefinitionId{
                                (static_cast<std::uint32_t>(
                                     lispb::schema::TypeDefinitionKind::mini_float)
                                 << 24) |
                                0xffffffu};
                        }
                        case lispb::codegen::DeclarationKind::optional_sentinel: {

                            return lispb::schema::TypeDefinitionId{
                                (static_cast<std::uint32_t>(
                                     lispb::schema::TypeDefinitionKind::optional_sentinel)
                                 << 24) |
                                0xffffffu};
                        }
                        case lispb::codegen::DeclarationKind::optional_presence_bit: {

                            return lispb::schema::TypeDefinitionId{
                                (static_cast<std::uint32_t>(
                                     lispb::schema::TypeDefinitionKind::optional_presence_bit)
                                 << 24) |
                                0xffffffu};
                        }
                        case lispb::codegen::DeclarationKind::packed_value: {

                            return lispb::schema::TypeDefinitionId{
                                (static_cast<std::uint32_t>(
                                     lispb::schema::TypeDefinitionKind::packed)
                                 << 24) |
                                0xffffffu};
                        }
                        case lispb::codegen::DeclarationKind::record: {

                            return lispb::schema::TypeDefinitionId{
                                (static_cast<std::uint32_t>(
                                     lispb::schema::TypeDefinitionKind::record)
                                 << 24) |
                                0xffffffu};
                        }
                        case lispb::codegen::DeclarationKind::union_type: {

                            return lispb::schema::TypeDefinitionId{
                                (static_cast<std::uint32_t>(
                                     lispb::schema::TypeDefinitionKind::union_type)
                                 << 24) |
                                0xffffffu};
                        }
                        case lispb::codegen::DeclarationKind::tagged_union: {

                            return lispb::schema::TypeDefinitionId{
                                (static_cast<std::uint32_t>(
                                     lispb::schema::TypeDefinitionKind::tagged_union)
                                 << 24) |
                                0xffffffu};
                        }
                        case lispb::codegen::DeclarationKind::static_table: {

                            return lispb::schema::TypeDefinitionId{
                                (static_cast<std::uint32_t>(
                                     lispb::schema::TypeDefinitionKind::static_table)
                                 << 24) |
                                0xffffffu};
                        }
                        case lispb::codegen::DeclarationKind::facade: {

                            return lispb::schema::TypeDefinitionId{
                                (static_cast<std::uint32_t>(
                                     lispb::schema::TypeDefinitionKind::facade)
                                 << 24) |
                                0xffffffu};
                        }
                        case lispb::codegen::DeclarationKind::homogeneous_layout: {

                            return lispb::schema::TypeDefinitionId{
                                (static_cast<std::uint32_t>(lispb::schema::TypeDefinitionKind::soa)
                                 << 24) |
                                0xffffffu};
                        }
                        case lispb::codegen::DeclarationKind::soa: {

                            return lispb::schema::TypeDefinitionId{
                                (static_cast<std::uint32_t>(lispb::schema::TypeDefinitionKind::soa)
                                 << 24) |
                                0xffffffu};
                        }
                        case lispb::codegen::DeclarationKind::vector_soa: {

                            return lispb::schema::TypeDefinitionId{
                                (static_cast<std::uint32_t>(lispb::schema::TypeDefinitionKind::soa)
                                 << 24) |
                                0xffffffu};
                        }
                        default:
                            std::unreachable();
                    }
                }()};
                std::optional<IdentifierId> physical;
                if (auto const soa{((source).kind() == lispb::codegen::DeclarationKind::soa
                                        ? std::optional{source}
                                        : std::nullopt)}) {
                    if (store_.soas.layout_only[store_.soas.index(*soa)]) {
                        physical = IdentifierId{};
                    } else if (!store_.soas.emits_vector_storage(*soa) &&
                               store_.soas.single_allocation[store_.soas.index(*soa)]) {
                        physical = *store_.soas.single_allocation[store_.soas.index(*soa)];
                    }
                }
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
                       std::span<IdentifierId const> const names = {}) -> TypeDefinitionId {
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
                    string_id(spelling),
                    external_type(manifest_.types.registered
                                      .cpp_type[manifest_.types.registered.index(registered)],
                                  manifest_.types.registered
                                      .semantics[manifest_.types.registered.index(registered)],
                                  std::span{&name, 1}))};
                graph_.registered_types_.emplace(name, id);
            }
        }
    }

    auto raw_external(IdentifierId const name) -> TypeId {
        if (auto const found{raw_external_types_.find(name)}; found != raw_external_types_.end()) {
            return found->second;
        }
        auto const metadata{codegen::external_scalar_schema(manifest_.types, str(name))};
        // Keep occurrence qualifiers and emission dependencies on the resolved use.
        auto const cpp_type{manifest_.types.cpp_types.intern(store_.strings().view(name))};
        auto const id{
            add_type({.origin = TypeOrigin::cpp_spelling, .name = name},
                     string_id(name),
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
        auto resolved{codegen::resolve_type_use(reference_id, manifest_.types)};
        auto const found{graph_.find_reference(reference_id, module)};
        auto const type{found.value_or(TypeId{})};
        auto semantic_type{type};
        if (found) {
            auto const node{type};
            if (((graph_.definition(node)).kind() == lispb::schema::TypeDefinitionKind::soa)) {
                auto const module_settings{store_.module_settings(graph_.module(node))};
                if (graph_.cpp_spelling(node) !=
                    string_id(qualified_name(module_settings, graph_.name(node)))) {
                    if (store_.strings().view(graph_.cpp_spelling(node)).empty()) {
                        resolved.cpp_type = manifest_.types.cpp_types.intern("");
                        resolved.physical = {};
                    } else {
                        auto spelling{str(graph_.cpp_spelling(node))};
                        if (store_.type_refs.nested[store_.type_refs.index(reference)]) {
                            spelling +=
                                "::" +
                                str(*store_.type_refs.nested[store_.type_refs.index(reference)]);
                        }
                        spelling += str(store_.type_refs.suffix[store_.type_refs.index(reference)]);
                        auto& types{manifest_.types.cpp_types};
                        resolved.cpp_type = types.derive(resolved.cpp_type, spelling);
                        auto const names_semantic_type{resolved.physical.names_semantic_type};
                        resolved.physical =
                            codegen::classify_physical_type_use(types, resolved.cpp_type);
                        resolved.physical.names_semantic_type = names_semantic_type;
                    }
                }
            }

        } else {
            assert(!store_.strings()
                        .view(store_.type_refs.name[store_.type_refs.index(reference)])
                        .starts_with('@'));
            semantic_type = raw_external(store_.type_refs.name[store_.type_refs.index(reference)]);
        }
        auto const result{add_occurrence(semantic_type, resolved.cpp_type, resolved.physical)};
        graph_.bindings_.emplace(key, result);
        return result;
    }

    auto resolve_ref(codegen::TypeRefId const reference, codegen::ModuleHandle const module) const
        -> ResolvedOccurrenceId {
        return graph_.resolved(module, reference);
    }
    auto local_declaration(codegen::ModuleHandle const module, IdentifierId const name) const
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
        auto const reference{reference_id};
        if (graph_.module(node) == module ||
            store_.strings()
                .view(store_.type_refs.name[store_.type_refs.index(reference)])
                .starts_with('@') ||
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
        auto const cpp_type{types.derive(graph_.occurrences().cpp_type[row],
                                         types.spelling(graph_.occurrences().cpp_type[row]),
                                         dependencies)};
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
                for (auto const declaration : store_.items(
                         store_.normal_modules.declarations[store_.normal_modules.index(module)])) {
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
                            if (declaration.kind() == codegen::DeclarationKind::soa &&
                                role.category == codegen::TypeReferenceCategory::member &&
                                role.part == codegen::TypeReferencePart::direct) {
                                auto const soa{declaration};
                                auto const member{store_.items(
                                    store_.soas.members[store_.soas.index(soa)])[role.index]};
                                emitted =
                                    store_.soa_members.kind[store_.soa_members.index(member)] !=
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
                                            IdentifierId const name) {
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
                                for (auto const name : store_.items(
                                         store_.functions
                                             .dependencies[store_.functions.index(method)])) {
                                    registration(
                                        {.category = category,
                                         .part = codegen::TypeReferencePart::dependency,
                                         .subject =
                                             store_.functions.name[store_.functions.index(method)],
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
                        for (auto const name : store_.items(
                                 store_.facades
                                     .validation_dependencies[store_.facades.index(*facade)])) {
                            registration(
                                {.category = codegen::TypeReferenceCategory::validation_dependency},
                                name);
                        }
                    } else if (auto const record{
                                   ((declaration).kind() == lispb::codegen::DeclarationKind::record
                                        ? std::optional{declaration}
                                        : std::nullopt)}) {
                        functions(codegen::TypeReferenceCategory::function,
                                  store_.records.functions[store_.records.index(*record)]);
                    } else if (auto const soa{
                                   ((declaration).kind() == lispb::codegen::DeclarationKind::soa
                                        ? std::optional{declaration}
                                        : std::nullopt)}) {
                        functions(codegen::TypeReferenceCategory::function,
                                  store_.soas.functions[store_.soas.index(*soa)]);
                        functions(codegen::TypeReferenceCategory::const_view_function,
                                  store_.soas.const_view_functions[store_.soas.index(*soa)]);
                        functions(codegen::TypeReferenceCategory::view_function,
                                  store_.soas.mutable_view_functions[store_.soas.index(*soa)]);
                    }
                }
                std::uint32_t index{};
                for (auto const allocator_id :
                     store_.items(store_.normal_modules
                                      .soa_array_allocators[store_.normal_modules.index(module)])) {
                    auto const allocator{allocator_id};
                    auto const target{bind_reference(
                        store_.soa_allocator_variants
                            .allocator[store_.soa_allocator_variants.index(allocator)],
                        module_id)};
                    add_use(module_id,
                            {},
                            {.category = codegen::TypeReferenceCategory::module_allocator,
                             .index = index++},
                            target,
                            store_.soa_allocator_variants
                                .allocator[store_.soa_allocator_variants.index(allocator)]);
                }
            } else if (module_id.kind() == codegen::ModuleKind::settings) {
                auto const module{module_id};
                std::uint32_t index{};
                for (auto const setting_id :
                     store_.items(store_.settings_modules
                                      .settings_list[store_.settings_modules.index(module)])) {
                    auto const setting{setting_id};
                    auto const target{bind_reference(
                        store_.settings.value_type[store_.settings.index(setting)], module_id)};
                    add_use(module_id,
                            {},
                            {.category = codegen::TypeReferenceCategory::setting,
                             .subject = store_.settings.name[store_.settings.index(setting)],
                             .index = index++},
                            target,
                            store_.settings.value_type[store_.settings.index(setting)]);
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
        auto const target{resolve_ref(
            store_.semantic_relations.target[store_.semantic_relations.index(relation)], module)};
        if (graph_.origin(graph_.occurrences().type[graph_.occurrences().index(target)]) !=
            TypeOrigin::declaration) {
            throw std::invalid_argument{
                std::string{context} + " relationship target '" +
                str(store_.type_refs.name[store_.type_refs.index(
                    store_.semantic_relations.target[store_.semantic_relations.index(relation)])]) +
                "' must resolve to a declared semantic type"};
        }
        auto& relationships{graph_.relationships_};
        auto const id{relationships.append()};
        auto const row{relationships.index(id)};
        relationships.kind[row] =
            store_.semantic_relations.kind[store_.semantic_relations.index(relation)];
        relationships.target[row] = target;
        relationships.unit[row] =
            store_.semantic_relations.unit[store_.semantic_relations.index(relation)];
        relationships_by_source_.emplace(key, id);
        return id;
    }

    auto resolve_normal_definition(codegen::ModuleHandle const module_id,
                                   codegen::DeclarationHandle const declaration)
        -> TypeDefinitionId {
        auto const module{module_id};
        auto& definitions{graph_.definitions_};
        return [&]() -> TypeDefinitionId {
            auto const source{declaration};
            switch (source.kind()) {
                case lispb::codegen::DeclarationKind::enumeration: {

                    auto const new_enums_3{definitions.enums.append()};
                    definitions.enums.domain[definitions.enums.index(new_enums_3)] =
                        graph_.enum_domains_.find(source);
                    definitions.enums.underlying_type[definitions.enums.index(new_enums_3)] =
                        store_.enums.underlying_type[store_.enums.index(source)]
                            .transform([&](auto id) { return resolve_ref(id, module_id); })
                            .value_or(ResolvedOccurrenceId{});
                    definitions.enums.bit_width[definitions.enums.index(new_enums_3)] =
                        store_.enums.bit_width[store_.enums.index(source)];
                    definitions.enums.signedness[definitions.enums.index(new_enums_3)] =
                        store_.enums.signedness[store_.enums.index(source)];
                    definitions.enums.count[definitions.enums.index(new_enums_3)] =
                        store_.enums.count[store_.enums.index(source)].transform(
                            [](auto id) { return string_id(id); });
                    definitions.enums.enum_array[definitions.enums.index(new_enums_3)] =
                        store_.enums.enum_array[store_.enums.index(source)];
                    return new_enums_3;
                }
                case lispb::codegen::DeclarationKind::integer_scalar: {

                    auto minimum{
                        store_.integer_scalars.minimum_value[store_.integer_scalars.index(source)]};
                    auto maximum{
                        store_.integer_scalars.maximum_value[store_.integer_scalars.index(source)]};
                    for (auto const code_id :
                         store_.items(store_.integer_scalars
                                          .named_codes[store_.integer_scalars.index(source)])) {
                        auto const code{code_id};
                        if (codegen::packed_integer_less(
                                store_.packed_named_codes
                                    .value[store_.packed_named_codes.index(code)],
                                minimum)) {
                            minimum = store_.packed_named_codes
                                          .value[store_.packed_named_codes.index(code)];
                        }
                        if (codegen::packed_integer_less(
                                maximum,
                                store_.packed_named_codes
                                    .value[store_.packed_named_codes.index(code)])) {
                            maximum = store_.packed_named_codes
                                          .value[store_.packed_named_codes.index(code)];
                        }
                    }
                    auto& scalars{definitions.integer_scalars};
                    auto const result{scalars.append()};
                    auto const scalar_index{scalars.index(result)};
                    scalars.signedness[scalar_index] =
                        store_.integer_scalars.signedness[store_.integer_scalars.index(source)];
                    scalars.minimum_value[scalar_index] =
                        store_.integer_scalars.minimum_value[store_.integer_scalars.index(source)];
                    scalars.maximum_value[scalar_index] =
                        store_.integer_scalars.maximum_value[store_.integer_scalars.index(source)];
                    scalars.bit_width[scalar_index] =
                        store_.integer_scalars.bit_width[store_.integer_scalars.index(source)]
                            .value_or(*codegen::minimum_packed_integer_bits(
                                minimum,
                                maximum,
                                store_.integer_scalars
                                    .signedness[store_.integer_scalars.index(source)]));
                    scalars.bit_width_auto[scalar_index] =
                        !store_.integer_scalars.bit_width[store_.integer_scalars.index(source)];
                    scalars.named_codes[scalar_index] =
                        store_.integer_scalars.named_codes[store_.integer_scalars.index(source)];
                    scalars.relationship[scalar_index] = relationship(
                        store_.integer_scalars.relationship[store_.integer_scalars.index(source)],
                        module_id,
                        "Integer scalar");
                    if (store_.integer_scalars.cpp_emission[store_.integer_scalars.index(source)] ==
                            codegen::IntegerScalarCppEmission::alias &&
                        store_.integer_scalars.cpp_type[store_.integer_scalars.index(source)]) {
                        scalars.cpp_representation[scalar_index] = resolve_ref(
                            *store_.integer_scalars.cpp_type[store_.integer_scalars.index(source)],
                            module_id);
                    }
                    return result;
                }
                case lispb::codegen::DeclarationKind::linear_quantized: {

                    auto const scalar{resolve_ref(
                        store_.linear_quantizeds.source[store_.linear_quantizeds.index(source)],
                        module_id)};
                    if (!integer_domain(
                            graph_,
                            graph_.occurrences().type[graph_.occurrences().index(scalar)])) {
                        throw std::invalid_argument{
                            "Linear quantization '" +
                            str(store_.linear_quantizeds
                                    .name[store_.linear_quantizeds.index(source)]) +
                            "' source must have a declared integer domain"};
                    }
                    auto const new_linear_quantized_4{definitions.linear_quantized.append()};
                    definitions.linear_quantized
                        .source[definitions.linear_quantized.index(new_linear_quantized_4)] =
                        scalar;
                    definitions.linear_quantized
                        .bit_width[definitions.linear_quantized.index(new_linear_quantized_4)] =
                        store_.linear_quantizeds.bit_width[store_.linear_quantizeds.index(source)];
                    definitions.linear_quantized.reserved_codes[definitions.linear_quantized.index(
                        new_linear_quantized_4)] =
                        store_.linear_quantizeds
                            .reserved_codes[store_.linear_quantizeds.index(source)];
                    definitions.linear_quantized
                        .clipping[definitions.linear_quantized.index(new_linear_quantized_4)] =
                        store_.linear_quantizeds.clipping[store_.linear_quantizeds.index(source)];
                    return new_linear_quantized_4;
                }
                case lispb::codegen::DeclarationKind::integer_varint: {

                    auto const scalar{resolve_ref(
                        store_.integer_varints.source[store_.integer_varints.index(source)],
                        module_id)};
                    if (!integer_domain(
                            graph_,
                            graph_.occurrences().type[graph_.occurrences().index(scalar)])) {
                        throw std::invalid_argument{
                            "Integer varint '" +
                            str(store_.integer_varints.name[store_.integer_varints.index(source)]) +
                            "' source must have a declared integer domain"};
                    }
                    auto const new_integer_varints_5{definitions.integer_varints.append()};
                    definitions.integer_varints
                        .source[definitions.integer_varints.index(new_integer_varints_5)] = scalar;
                    definitions.integer_varints
                        .encoding[definitions.integer_varints.index(new_integer_varints_5)] =
                        store_.integer_varints.encoding[store_.integer_varints.index(source)];
                    return new_integer_varints_5;
                }
                case lispb::codegen::DeclarationKind::fixed_point: {

                    auto const new_fixed_points_6{definitions.fixed_points.append()};
                    definitions.fixed_points
                        .signedness[definitions.fixed_points.index(new_fixed_points_6)] =
                        store_.fixed_points.signedness[store_.fixed_points.index(source)];
                    definitions.fixed_points
                        .total_bits[definitions.fixed_points.index(new_fixed_points_6)] =
                        store_.fixed_points.total_bits[store_.fixed_points.index(source)];
                    definitions.fixed_points
                        .fractional_bits[definitions.fixed_points.index(new_fixed_points_6)] =
                        store_.fixed_points.fractional_bits[store_.fixed_points.index(source)];
                    definitions.fixed_points
                        .rounding[definitions.fixed_points.index(new_fixed_points_6)] =
                        store_.fixed_points.rounding[store_.fixed_points.index(source)];
                    definitions.fixed_points
                        .minimum_raw_value[definitions.fixed_points.index(new_fixed_points_6)] =
                        store_.fixed_points.minimum_value[store_.fixed_points.index(source)]
                            .transform([&](auto id) {
                                return *codegen::parse_fixed_point_value(
                                    store_.strings().view(id),
                                    store_.fixed_points
                                        .fractional_bits[store_.fixed_points.index(source)]);
                            });
                    definitions.fixed_points
                        .maximum_raw_value[definitions.fixed_points.index(new_fixed_points_6)] =
                        store_.fixed_points.maximum_value[store_.fixed_points.index(source)]
                            .transform([&](auto id) {
                                return *codegen::parse_fixed_point_value(
                                    store_.strings().view(id),
                                    store_.fixed_points
                                        .fractional_bits[store_.fixed_points.index(source)]);
                            });
                    return new_fixed_points_6;
                }
                case lispb::codegen::DeclarationKind::mini_float: {

                    auto const new_mini_floats_7{definitions.mini_floats.append()};
                    definitions.mini_floats
                        .sign_bits[definitions.mini_floats.index(new_mini_floats_7)] =
                        store_.mini_floats.sign_bits[store_.mini_floats.index(source)];
                    definitions.mini_floats
                        .exponent_bits[definitions.mini_floats.index(new_mini_floats_7)] =
                        store_.mini_floats.exponent_bits[store_.mini_floats.index(source)];
                    definitions.mini_floats
                        .significand_bits[definitions.mini_floats.index(new_mini_floats_7)] =
                        store_.mini_floats.significand_bits[store_.mini_floats.index(source)];
                    definitions.mini_floats
                        .exponent_bias[definitions.mini_floats.index(new_mini_floats_7)] =
                        store_.mini_floats.exponent_bias[store_.mini_floats.index(source)];
                    return new_mini_floats_7;
                }
                case lispb::codegen::DeclarationKind::optional_sentinel: {

                    auto const resolved{resolve_ref(
                        store_.optional_sentinels.source[store_.optional_sentinels.index(source)],
                        module_id)};
                    auto const scalar{integer_domain(
                        graph_, graph_.occurrences().type[graph_.occurrences().index(resolved)])};
                    if (!scalar) {
                        throw std::invalid_argument{
                            "Optional sentinel representation '" +
                            str(store_.optional_sentinels
                                    .name[store_.optional_sentinels.index(source)]) +
                            "' source must have a declared integer domain"};
                    }
                    auto const codes{store_.items(
                        graph_.definitions()
                            .integer_scalars
                            .named_codes[graph_.definitions().integer_scalars.index(*scalar)])};
                    auto const sentinel{std::ranges::find_if(codes, [&](auto id) {
                        return store_.packed_named_codes
                                   .name[store_.packed_named_codes.index(id)] ==
                               store_.optional_sentinels
                                   .sentinel[store_.optional_sentinels.index(source)];
                    })};
                    if (sentinel == codes.end() ||
                        !store_.packed_named_codes
                             .sentinel[store_.packed_named_codes.index(*sentinel)]) {
                        throw std::invalid_argument{
                            "Optional sentinel representation '" +
                            str(store_.optional_sentinels
                                    .name[store_.optional_sentinels.index(source)]) +
                            "' must name a source sentinel code"};
                    }
                    auto const new_optional_sentinels_8{definitions.optional_sentinels.append()};
                    definitions.optional_sentinels
                        .source[definitions.optional_sentinels.index(new_optional_sentinels_8)] =
                        resolved;
                    definitions.optional_sentinels
                        .sentinel_name[definitions.optional_sentinels.index(
                            new_optional_sentinels_8)] =
                        string_id(store_.optional_sentinels
                                      .sentinel[store_.optional_sentinels.index(source)]);
                    definitions.optional_sentinels
                        .sentinel_value[definitions.optional_sentinels.index(
                            new_optional_sentinels_8)] =
                        store_.packed_named_codes.value[store_.packed_named_codes.index(*sentinel)];
                    definitions.optional_sentinels
                        .bit_width[definitions.optional_sentinels.index(new_optional_sentinels_8)] =
                        graph_.definitions()
                            .integer_scalars
                            .bit_width[graph_.definitions().integer_scalars.index(*scalar)];
                    return new_optional_sentinels_8;
                }
                case lispb::codegen::DeclarationKind::optional_presence_bit: {

                    auto const resolved{
                        resolve_ref(store_.optional_presence_bits
                                        .source[store_.optional_presence_bits.index(source)],
                                    module_id)};
                    auto const scalar{integer_domain(
                        graph_, graph_.occurrences().type[graph_.occurrences().index(resolved)])};
                    if (!scalar) {
                        throw std::invalid_argument{
                            "Optional presence-bit representation '" +
                            str(store_.optional_presence_bits
                                    .name[store_.optional_presence_bits.index(source)]) +
                            "' source must have a declared integer domain"};
                    }
                    auto const new_optional_presence_bits_9{
                        definitions.optional_presence_bits.append()};
                    definitions.optional_presence_bits
                        .source[definitions.optional_presence_bits.index(
                            new_optional_presence_bits_9)] = resolved;
                    definitions.optional_presence_bits
                        .payload_bits[definitions.optional_presence_bits.index(
                            new_optional_presence_bits_9)] =
                        graph_.definitions()
                            .integer_scalars
                            .bit_width[graph_.definitions().integer_scalars.index(*scalar)];
                    definitions.optional_presence_bits
                        .encoded_bits[definitions.optional_presence_bits.index(
                            new_optional_presence_bits_9)] =
                        graph_.definitions()
                            .integer_scalars
                            .bit_width[graph_.definitions().integer_scalars.index(*scalar)] +
                        1;
                    return new_optional_presence_bits_9;
                }
                case lispb::codegen::DeclarationKind::packed_value: {

                    std::vector<PackedSegmentId> segments;
                    for (auto const segment_id : store_.items(
                             store_.packed_values.segments[store_.packed_values.index(source)])) {
                        if (auto const value{
                                ((segment_id).kind() == lispb::codegen::PackedSegmentKind::field
                                     ? std::optional{segment_id}
                                     : std::nullopt)}) {
                            auto const scalar{codegen::detail::find_packed_integer_domain(
                                *value, graph_, module_id)};
                            auto const new_packed_fields_10{definitions.packed_fields.append()};
                            definitions.packed_fields
                                .source[definitions.packed_fields.index(new_packed_fields_10)] =
                                segment_id;
                            definitions.packed_fields
                                .name[definitions.packed_fields.index(new_packed_fields_10)] =
                                store_.packed_fields.name[store_.packed_fields.index(*value)];
                            definitions.packed_fields.semantic_type[definitions.packed_fields.index(
                                new_packed_fields_10)] =
                                resolve_ref(
                                    store_.packed_fields.type[store_.packed_fields.index(*value)],
                                    module_id);
                            definitions.packed_fields
                                .bit_width[definitions.packed_fields.index(new_packed_fields_10)] =
                                static_cast<std::uint32_t>(
                                    *codegen::detail::derive_packed_field_width(
                                        *value, manifest_, graph_, module_id));
                            definitions.packed_fields
                                .bit_width_auto[definitions.packed_fields.index(
                                    new_packed_fields_10)] =
                                !store_.packed_fields.bits[store_.packed_fields.index(*value)];
                            definitions.packed_fields
                                .kind[definitions.packed_fields.index(new_packed_fields_10)] =
                                store_.packed_fields.kind[store_.packed_fields.index(*value)];
                            definitions.packed_fields.range_helper[definitions.packed_fields.index(
                                new_packed_fields_10)] =
                                store_.packed_fields
                                    .range_helper[store_.packed_fields.index(*value)];
                            definitions.packed_fields.minimum_value[definitions.packed_fields.index(
                                new_packed_fields_10)] =
                                scalar
                                    ? std::optional{definitions.integer_scalars
                                                        .minimum_value[definitions.integer_scalars
                                                                           .index(*scalar)]}
                                    : store_.packed_fields
                                          .minimum_value[store_.packed_fields.index(*value)];
                            definitions.packed_fields.maximum_value[definitions.packed_fields.index(
                                new_packed_fields_10)] =
                                scalar
                                    ? std::optional{definitions.integer_scalars
                                                        .maximum_value[definitions.integer_scalars
                                                                           .index(*scalar)]}
                                    : store_.packed_fields
                                          .maximum_value[store_.packed_fields.index(*value)];
                            definitions.packed_fields.named_codes[definitions.packed_fields.index(
                                new_packed_fields_10)] =
                                scalar
                                    ? definitions.integer_scalars
                                          .named_codes[definitions.integer_scalars.index(*scalar)]
                                    : store_.packed_fields
                                          .named_codes[store_.packed_fields.index(*value)];
                            definitions.packed_fields.relationship[definitions.packed_fields.index(
                                new_packed_fields_10)] =
                                relationship(store_.packed_fields
                                                 .relationship[store_.packed_fields.index(*value)],
                                             module_id,
                                             "Packed field");
                            definitions.packed_fields.default_value[definitions.packed_fields.index(
                                new_packed_fields_10)] =
                                store_.packed_fields
                                    .default_value[store_.packed_fields.index(*value)];
                            segments.push_back(new_packed_fields_10);
                        } else {
                            auto const reserved{segment_id};
                            auto const new_packed_reserved_bits_11{
                                definitions.packed_reserved_bits.append()};
                            definitions.packed_reserved_bits
                                .name[definitions.packed_reserved_bits.index(
                                    new_packed_reserved_bits_11)] =
                                store_.packed_reserved_bits
                                    .name[store_.packed_reserved_bits.index(reserved)];
                            definitions.packed_reserved_bits
                                .bit_width[definitions.packed_reserved_bits.index(
                                    new_packed_reserved_bits_11)] =
                                static_cast<std::uint32_t>(
                                    store_.packed_reserved_bits
                                        .bits[store_.packed_reserved_bits.index(reserved)]);
                            segments.push_back(new_packed_reserved_bits_11);
                        }
                    }
                    auto const new_packed_12{definitions.packed.append()};
                    definitions.packed.storage_type[definitions.packed.index(new_packed_12)] =
                        resolve_ref(
                            store_.packed_values.storage_type[store_.packed_values.index(source)],
                            module_id);
                    definitions.packed.set_segments(new_packed_12, segments);
                    definitions.packed.invalid_raw_value[definitions.packed.index(new_packed_12)] =
                        store_.packed_values.invalid_value[store_.packed_values.index(source)];
                    definitions.packed.byte_order[definitions.packed.index(new_packed_12)] =
                        store_.packed_values.byte_order[store_.packed_values.index(source)];
                    definitions.packed.bit_order[definitions.packed.index(new_packed_12)] =
                        store_.packed_values.bit_order[store_.packed_values.index(source)].value_or(
                            codegen::PackedBitOrder::least_significant_first);
                    definitions.packed.default_raw_value[definitions.packed.index(new_packed_12)] =
                        codegen::detail::packed_default_value(source, manifest_, graph_, module_id);
                    return new_packed_12;
                }
                case lispb::codegen::DeclarationKind::record: {

                    std::vector<RecordMemberId> members;
                    for (auto const member_id :
                         store_.items(store_.records.members[store_.records.index(source)])) {
                        auto const member{member_id};
                        auto const new_record_members_13{definitions.record_members.append()};
                        definitions.record_members
                            .name[definitions.record_members.index(new_record_members_13)] =
                            store_.record_members.name[store_.record_members.index(member)];
                        definitions.record_members.semantic_type[definitions.record_members.index(
                            new_record_members_13)] =
                            resolve_ref(
                                store_.record_members.type[store_.record_members.index(member)],
                                module_id);
                        definitions.record_members
                            .count[definitions.record_members.index(new_record_members_13)] =
                            store_.record_members.count[store_.record_members.index(member)];
                        definitions.record_members
                            .relationship[definitions.record_members.index(new_record_members_13)] =
                            relationship(store_.record_members
                                             .relationship[store_.record_members.index(member)],
                                         module_id,
                                         "Record member");
                        definitions.record_members
                            .initializer[definitions.record_members.index(new_record_members_13)] =
                            store_.record_members.initializer[store_.record_members.index(member)];
                        members.push_back(new_record_members_13);
                    }
                    auto const new_records_14{definitions.records.append()};
                    definitions.records.set_members(new_records_14, members);
                    return new_records_14;
                }
                case lispb::codegen::DeclarationKind::union_type: {

                    std::vector<UnionAlternativeId> alternatives;
                    for (auto const alternative_id :
                         store_.items(store_.unions.alternatives[store_.unions.index(source)])) {
                        auto const alternative{alternative_id};
                        auto const new_union_alternatives_15{
                            definitions.union_alternatives.append()};
                        definitions.union_alternatives
                            .name[definitions.union_alternatives.index(new_union_alternatives_15)] =
                            store_.union_alternatives
                                .name[store_.union_alternatives.index(alternative)];
                        definitions.union_alternatives
                            .semantic_type[definitions.union_alternatives.index(
                                new_union_alternatives_15)] =
                            resolve_ref(store_.union_alternatives
                                            .type[store_.union_alternatives.index(alternative)],
                                        module_id);
                        definitions.union_alternatives.count[definitions.union_alternatives.index(
                            new_union_alternatives_15)] =
                            store_.union_alternatives
                                .count[store_.union_alternatives.index(alternative)];
                        alternatives.push_back(new_union_alternatives_15);
                    }
                    auto const new_unions_16{definitions.unions.append()};
                    definitions.unions.set_alternatives(new_unions_16, alternatives);
                    return new_unions_16;
                }
                case lispb::codegen::DeclarationKind::tagged_union: {

                    auto const discriminant{resolve_ref(
                        store_.tagged_unions.discriminant[store_.tagged_unions.index(source)],
                        module_id)};
                    auto const definition{graph_.definition(
                        graph_.occurrences().type[graph_.occurrences().index(discriminant)])};
                    if (!((definition).kind() == lispb::schema::TypeDefinitionKind::enumeration)) {
                        throw std::invalid_argument{
                            "Tagged union '" +
                            str(store_.tagged_unions.name[store_.tagged_unions.index(source)]) +
                            "' discriminant must resolve to an enum declaration"};
                    }
                    auto const enumeration{definition};
                    auto const domain{
                        definitions.enums.domain[definitions.enums.index(enumeration)]};
                    auto const enumerators{graph_.enum_domains_.values(domain)};
                    std::vector<TaggedUnionAlternativeId> alternatives;
                    for (auto const alternative_id :
                         store_.items(store_.tagged_unions
                                          .alternatives[store_.tagged_unions.index(source)])) {
                        auto const alternative{alternative_id};
                        auto const tag{std::ranges::find_if(enumerators, [&](auto id) {
                            return store_.enumerators.name[store_.enumerators.index(id)] ==
                                   store_.tagged_union_alternatives
                                       .tag[store_.tagged_union_alternatives.index(alternative)];
                        })};
                        if (tag == enumerators.end()) {
                            throw std::invalid_argument{
                                "Tagged union '" +
                                str(store_.tagged_unions.name[store_.tagged_unions.index(source)]) +
                                "' alternative '" +
                                str(store_.tagged_union_alternatives
                                        .name[store_.tagged_union_alternatives.index(
                                            alternative)]) +
                                "' maps unknown discriminant tag '" +
                                str(store_.tagged_union_alternatives
                                        .tag[store_.tagged_union_alternatives.index(alternative)]) +
                                "'"};
                        }
                        auto const value{*tag};
                        if (store_.enumerators.sentinel[store_.enumerators.index(value)] ||
                            value == graph_.enum_domains_
                                         .count_value[graph_.enum_domains_.index(domain)]) {
                            throw std::invalid_argument{
                                "Tagged union '" +
                                str(store_.tagged_unions.name[store_.tagged_unions.index(source)]) +
                                "' alternative '" +
                                str(store_.tagged_union_alternatives
                                        .name[store_.tagged_union_alternatives.index(
                                            alternative)]) +
                                "' cannot map sentinel tag '" +
                                str(store_.tagged_union_alternatives
                                        .tag[store_.tagged_union_alternatives.index(alternative)]) +
                                "'"};
                        }
                        auto const new_tagged_union_alternatives_17{
                            definitions.tagged_union_alternatives.append()};
                        definitions.tagged_union_alternatives
                            .name[definitions.tagged_union_alternatives.index(
                                new_tagged_union_alternatives_17)] =
                            store_.tagged_union_alternatives
                                .name[store_.tagged_union_alternatives.index(alternative)];
                        definitions.tagged_union_alternatives
                            .semantic_type[definitions.tagged_union_alternatives.index(
                                new_tagged_union_alternatives_17)] =
                            resolve_ref(
                                store_.tagged_union_alternatives
                                    .type[store_.tagged_union_alternatives.index(alternative)],
                                module_id);
                        definitions.tagged_union_alternatives
                            .count[definitions.tagged_union_alternatives.index(
                                new_tagged_union_alternatives_17)] =
                            store_.tagged_union_alternatives
                                .count[store_.tagged_union_alternatives.index(alternative)];
                        definitions.tagged_union_alternatives
                            .tag[definitions.tagged_union_alternatives.index(
                                new_tagged_union_alternatives_17)] =
                            store_.tagged_union_alternatives
                                .tag[store_.tagged_union_alternatives.index(alternative)];
                        alternatives.push_back(new_tagged_union_alternatives_17);
                    }
                    auto const new_tagged_unions_18{definitions.tagged_unions.append()};
                    definitions.tagged_unions
                        .discriminant[definitions.tagged_unions.index(new_tagged_unions_18)] =
                        discriminant;
                    definitions.tagged_unions.set_alternatives(new_tagged_unions_18, alternatives);
                    return new_tagged_unions_18;
                }
                case lispb::codegen::DeclarationKind::soa: {

                    std::vector<SoaColumnId> columns;
                    std::optional<TypeId> element;
                    for (auto const member_id :
                         store_.items(store_.soas.members[store_.soas.index(source)])) {
                        auto const member{member_id};
                        std::optional<TypeId> nested;
                        if (store_.soa_members.nested_schema[store_.soa_members.index(member)]) {
                            nested = local_declaration(
                                module_id,
                                *store_.soa_members
                                     .nested_schema[store_.soa_members.index(member)]);
                            if (!nested) {
                                throw std::invalid_argument{
                                    "Unknown nested semantic type '" +
                                    str(*store_.soa_members
                                             .nested_schema[store_.soa_members.index(member)]) +
                                    "' in module '" +
                                    str(store_.module_settings_store
                                            .name[store_.module_settings_store.index(
                                                store_.module_settings(module_id))]) +
                                    "'"};
                            }
                        }
                        auto const resolved{resolve_ref(
                            store_.soa_members.type[store_.soa_members.index(member)], module_id)};
                        if (!store_.soas.vector_components[store_.soas.index(source)].empty()) {
                            if (element &&
                                *element != graph_.occurrences()
                                                .type[graph_.occurrences().index(resolved)]) {
                                throw std::invalid_argument{
                                    "SOA '" + str(store_.soas.name[store_.soas.index(source)]) +
                                    "' vector components must have the same resolved type"};
                            }
                            element =
                                graph_.occurrences().type[graph_.occurrences().index(resolved)];
                        }
                        auto const new_soa_columns_19{definitions.soa_columns.append()};
                        definitions.soa_columns
                            .source[definitions.soa_columns.index(new_soa_columns_19)] = member_id;
                        definitions.soa_columns
                            .name[definitions.soa_columns.index(new_soa_columns_19)] =
                            store_.soa_members.name[store_.soa_members.index(member)];
                        definitions.soa_columns
                            .semantic_type[definitions.soa_columns.index(new_soa_columns_19)] =
                            resolved;
                        definitions.soa_columns
                            .kind[definitions.soa_columns.index(new_soa_columns_19)] =
                            store_.soa_members.kind[store_.soa_members.index(member)];
                        definitions.soa_columns
                            .nested_type[definitions.soa_columns.index(new_soa_columns_19)] =
                            nested;
                        definitions.soa_columns
                            .relationship[definitions.soa_columns.index(new_soa_columns_19)] =
                            relationship(
                                store_.soa_members.relationship[store_.soa_members.index(member)],
                                module_id,
                                "SOA '" + str(store_.soas.name[store_.soas.index(source)]) +
                                    "' member '" +
                                    str(store_.soa_members.name[store_.soa_members.index(member)]) +
                                    "'");
                        columns.push_back(new_soa_columns_19);
                    }
                    if (!store_.soas.vector_components[store_.soas.index(source)].empty() &&
                        columns.empty()) {
                        throw std::invalid_argument{
                            "SOA '" + str(store_.soas.name[store_.soas.index(source)]) +
                            "' vector components require columns"};
                    }
                    auto const new_soas_20{definitions.soas.append()};
                    definitions.soas.backend[definitions.soas.index(new_soas_20)] =
                        store_.normal_modules.soa_backend[store_.normal_modules.index(module)];
                    definitions.soas.source_kind[definitions.soas.index(new_soas_20)] =
                        SoaSourceKind::structure;
                    definitions.soas.set_columns(new_soas_20, columns);
                    definitions.soas.equivalent_type[definitions.soas.index(new_soas_20)] =
                        store_.soas.equivalent_type[store_.soas.index(source)]
                            .transform([&](auto id) { return resolve_ref(id, module_id); })
                            .value_or(ResolvedOccurrenceId{});
                    definitions.soas.related_storage_name[definitions.soas.index(new_soas_20)] =
                        store_.soas.single_allocation[store_.soas.index(source)].transform(
                            [](auto id) { return string_id(id); });
                    definitions.soas.set_vector_components(
                        new_soas_20,
                        store_.items(store_.soas.vector_components[store_.soas.index(source)]));
                    definitions.soas.layout_only[definitions.soas.index(new_soas_20)] =
                        store_.soas.layout_only[store_.soas.index(source)];
                    return new_soas_20;
                }
                case lispb::codegen::DeclarationKind::vector_soa: {

                    auto const value{
                        resolve_ref(store_.vector_soas.value_type[store_.vector_soas.index(source)],
                                    module_id)};
                    std::vector<SoaColumnId> columns;
                    for (auto const component : store_.items(
                             store_.vector_soas.components[store_.vector_soas.index(source)])) {
                        auto const new_soa_columns_21{definitions.soa_columns.append()};
                        definitions.soa_columns
                            .name[definitions.soa_columns.index(new_soa_columns_21)] = component;
                        definitions.soa_columns
                            .semantic_type[definitions.soa_columns.index(new_soa_columns_21)] =
                            value;
                        columns.push_back(new_soa_columns_21);
                    }
                    auto const new_soas_22{definitions.soas.append()};
                    definitions.soas.backend[definitions.soas.index(new_soas_22)] =
                        store_.normal_modules.soa_backend[store_.normal_modules.index(module)];
                    definitions.soas.source_kind[definitions.soas.index(new_soas_22)] =
                        SoaSourceKind::vector;
                    definitions.soas.set_columns(new_soas_22, columns);
                    definitions.soas.equivalent_type[definitions.soas.index(new_soas_22)] =
                        resolve_ref(
                            store_.vector_soas.equivalent_type[store_.vector_soas.index(source)],
                            module_id);
                    definitions.soas.set_vector_components(
                        new_soas_22,
                        store_.items(
                            store_.vector_soas.components[store_.vector_soas.index(source)]));
                    definitions.soas.equivalent_constructor[definitions.soas.index(new_soas_22)] =
                        store_.vector_soas.equivalent_constructor[store_.vector_soas.index(source)];
                    return new_soas_22;
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
                case lispb::codegen::DeclarationKind::homogeneous_layout: {

                    std::unreachable();
                }
                default:
                    std::unreachable();
            }
        }();
    }

    void resolve_definitions(bool const primary_pass) {
        {
            for (std::size_t index{}; index < declarations_.size(); ++index) {
                auto const declaration{declarations_[index]};
                auto const source{graph_.source_declaration(declaration)};
                auto const primary{source.kind() == codegen::DeclarationKind::enumeration ||
                                   source.kind() == codegen::DeclarationKind::integer_scalar};
                if (primary != primary_pass) {
                    continue;
                }
                if (homogeneous_values_[index]) {
                    auto const layout{source};
                    auto const value{*homogeneous_values_[index]};
                    std::vector<ResolvedOccurrenceId> input_types;
                    for (auto const input :
                         store_.items(store_.homogeneous_values
                                          .input_types[store_.homogeneous_values.index(value)])) {
                        input_types.push_back(resolve_ref(input, graph_.module(declaration)));
                    }
                    auto const new_homogeneous_storage_29{
                        graph_.definitions_.homogeneous_storage.append()};
                    graph_.definitions_.homogeneous_storage.set_components(
                        new_homogeneous_storage_29,
                        store_.items(store_.homogeneous_layouts
                                         .components[store_.homogeneous_layouts.index(layout)]));
                    graph_.definitions_.homogeneous_storage
                        .value_type[graph_.definitions_.homogeneous_storage.index(
                            new_homogeneous_storage_29)] =
                        resolve_ref(
                            store_.homogeneous_values.type[store_.homogeneous_values.index(value)],
                            graph_.module(declaration));
                    graph_.definitions_.homogeneous_storage
                        .equivalent_type[graph_.definitions_.homogeneous_storage.index(
                            new_homogeneous_storage_29)] =
                        store_.homogeneous_values
                            .equivalent_type[store_.homogeneous_values.index(value)]
                            .transform([&](auto id) {
                                return resolve_ref(id, graph_.module(declaration));
                            })
                            .value_or(ResolvedOccurrenceId{});
                    graph_.definitions_.homogeneous_storage.set_input_members(
                        new_homogeneous_storage_29,
                        store_.items(store_.homogeneous_layouts
                                         .input_members[store_.homogeneous_layouts.index(layout)]));
                    graph_.definitions_.homogeneous_storage.set_input_types(
                        new_homogeneous_storage_29, input_types);
                    graph_.definitions_.homogeneous_storage
                        .view_template_name[graph_.definitions_.homogeneous_storage.index(
                            new_homogeneous_storage_29)] =
                        text("T" +
                             str(store_.homogeneous_layouts
                                     .name[store_.homogeneous_layouts.index(layout)]) +
                             "View");
                    graph_.definitions_ids_[declaration.value] = new_homogeneous_storage_29;
                } else {
                    graph_.definitions_ids_[declaration.value] =
                        resolve_normal_definition(graph_.module(declaration), source);
                }
            }
        }
    }

    void validate_nested_soa_types() const {
        std::set<TypeId> visited;
        auto visit = [&](auto&& self, TypeId const id) -> void {
            if (!visited.insert(id).second) {
                return;
            }
            auto const soa{graph_.definition(id)};
            for (auto const column_id : graph_.definitions_.soas.columns_of(soa)) {
                auto const column{column_id};
                if (!graph_.definitions_.soa_columns
                         .nested_type[graph_.definitions_.soa_columns.index(column)]) {
                    continue;
                }
                if (graph_.occurrences().type[graph_.occurrences().index(
                        graph_.definitions_.soa_columns
                            .semantic_type[graph_.definitions_.soa_columns.index(column)])] !=
                        *graph_.definitions_.soa_columns
                             .nested_type[graph_.definitions_.soa_columns.index(column)] &&
                    graph_.name(graph_.occurrences().type[graph_.occurrences().index(
                        graph_.definitions_.soa_columns
                            .semantic_type[graph_.definitions_.soa_columns.index(column)])]) !=
                        graph_.name(
                            *graph_.definitions_.soa_columns
                                 .nested_type[graph_.definitions_.soa_columns.index(column)])) {
                    throw std::invalid_argument{
                        "Nested SOA type does not match nested_schema: " +
                        str(graph_.definitions_.soa_columns
                                .name[graph_.definitions_.soa_columns.index(column)])};
                }
                self(self,
                     *graph_.definitions_.soa_columns
                          .nested_type[graph_.definitions_.soa_columns.index(column)]);
            }
        };
        auto const count{graph_.types().size()};
        for (std::uint32_t index{}; index < count; ++index) {
            auto const soa{
                ((graph_.definitions_ids_[index]).kind() == lispb::schema::TypeDefinitionKind::soa
                     ? std::optional{graph_.definitions_ids_[index]}
                     : std::nullopt)};
            if (soa &&
                graph_.definitions_.soas.source_kind[graph_.definitions_.soas.index(*soa)] ==
                    SoaSourceKind::structure &&
                graph_.definitions_.soas
                    .related_storage_name[graph_.definitions_.soas.index(*soa)]) {
                visit(visit, TypeId{index});
            }
        }
    }

    void validate_standard_library_soa_columns() const {
        auto const& soas{graph_.definitions_.soas};
        auto const& columns{graph_.definitions_.soa_columns};
        auto const& occurrences{graph_.occurrences()};
        std::set<TypeId> visited;
        auto visit = [&](auto&& self, TypeId const type) -> void {
            if (!visited.insert(type).second) {
                return;
            }
            for (auto const column : soas.columns_of(graph_.definition(type))) {
                auto const row{columns.index(column)};
                if (columns.nested_type[row]) {
                    self(self, *columns.nested_type[row]);
                    continue;
                }
                auto const occurrence{columns.semantic_type[row]};
                if (graph_.cpp_types().spelling(
                        occurrences.cpp_type[occurrences.index(occurrence)]) == "bool") {
                    throw std::invalid_argument{"Standard-library SoA requires contiguous columns; "
                                                "std::vector<bool> is unsupported"};
                }
            }
        };
        for (auto const type : graph_.types()) {
            auto const definition{graph_.definition(type)};
            if (definition.kind() != TypeDefinitionKind::soa) {
                continue;
            }
            auto const row{soas.index(definition)};
            if (soas.backend[row] != codegen::SoaBackend::standard_library ||
                soas.layout_only[row] ||
                (soas.source_kind[row] == SoaSourceKind::structure &&
                 !store_.soas.emits_vector_storage(graph_.source_declaration(type)))) {
                continue;
            }
            visit(visit, type);
        }
    }

    void validate_vector_equivalents() {
        std::map<TypeId, TypeDefinitionId> vectors;
        for (auto const node : graph_.types()) {
            auto const soa{
                ((graph_.definition(node)).kind() == lispb::schema::TypeDefinitionKind::soa
                     ? std::optional{graph_.definition(node)}
                     : std::nullopt)};
            if (soa &&
                graph_.definitions_.soas.source_kind[graph_.definitions_.soas.index(*soa)] ==
                    SoaSourceKind::vector &&
                is_valid(graph_.definitions_.soas
                             .equivalent_type[graph_.definitions_.soas.index(*soa)])) {
                vectors.emplace(graph_.occurrences().type[graph_.occurrences().index(
                                    graph_.definitions_.soas
                                        .equivalent_type[graph_.definitions_.soas.index(*soa)])],
                                graph_.definition(node));
            }
        }
        for (auto const node : graph_.types()) {
            auto const soa{
                ((graph_.definition(node)).kind() == lispb::schema::TypeDefinitionKind::soa
                     ? std::optional{graph_.definition(node)}
                     : std::nullopt)};
            if (!soa ||
                graph_.definitions_.soas.source_kind[graph_.definitions_.soas.index(*soa)] !=
                    SoaSourceKind::structure ||
                graph_.definitions_.soas.vector_components_of(*soa).empty() ||
                !is_valid(graph_.definitions_.soas
                              .equivalent_type[graph_.definitions_.soas.index(*soa)])) {
                continue;
            }
            auto const found{vectors.find(graph_.occurrences().type[graph_.occurrences().index(
                graph_.definitions_.soas.equivalent_type[graph_.definitions_.soas.index(*soa)])])};
            if (found == vectors.end()) {
                continue;
            }
            auto const declared{found->second};
            graph_.definitions_.soas
                .equivalent_constructor[graph_.definitions_.soas.index(graph_.definition(node))] =
                graph_.definitions_.soas
                    .equivalent_constructor[graph_.definitions_.soas.index(declared)];
            if (!std::ranges::equal(graph_.definitions_.soas.vector_components_of(*soa),
                                    graph_.definitions_.soas.vector_components_of(declared)) ||
                graph_.definitions_.soas.columns_of(*soa).size() !=
                    graph_.definitions_.soas.columns_of(declared).size()) {
                throw std::invalid_argument{
                    "SOA '" + str(graph_.name(node)) +
                    "' vector components disagree with its equivalent vector declaration"};
            }
            auto const count{graph_.definitions_.soas.columns_of(*soa).size()};
            for (std::size_t index{}; index < count; ++index) {
                if (graph_.occurrences().type[graph_.occurrences().index(
                        graph_.definitions_.soa_columns
                            .semantic_type[graph_.definitions_.soa_columns.index(
                                graph_.definitions_.soas.columns_of(*soa)[index])])] !=
                    graph_.occurrences().type[graph_.occurrences().index(
                        graph_.definitions_.soa_columns
                            .semantic_type[graph_.definitions_.soa_columns.index(
                                graph_.definitions_.soas.columns_of(declared)[index])])]) {
                    throw std::invalid_argument{
                        "SOA '" + str(graph_.name(node)) +
                        "' vector element type disagrees with its equivalent vector declaration"};
                }
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
        auto follow = [&](ResolvedOccurrenceId const reference) {
            if (graph_.occurrences().contains_value(reference)) {
                validate_aggregate_cycle(
                    graph_.occurrences().type[graph_.occurrences().index(reference)], states, path);
            }
        };
        switch ((graph_.definition(type)).kind()) {
            case lispb::schema::TypeDefinitionKind::record: {
                for (auto const id :
                     graph_.definitions_.records.members_of(graph_.definition(type))) {
                    follow(graph_.definitions_.record_members
                               .semantic_type[graph_.definitions_.record_members.index(id)]);
                }
                break;
            }
            case lispb::schema::TypeDefinitionKind::union_type: {
                for (auto const id :
                     graph_.definitions_.unions.alternatives_of(graph_.definition(type))) {
                    follow(graph_.definitions_.union_alternatives
                               .semantic_type[graph_.definitions_.union_alternatives.index(id)]);
                }
                break;
            }
            case lispb::schema::TypeDefinitionKind::tagged_union: {
                for (auto const id :
                     graph_.definitions_.tagged_unions.alternatives_of(graph_.definition(type))) {
                    follow(graph_.definitions_.tagged_union_alternatives
                               .semantic_type[graph_.definitions_.tagged_union_alternatives.index(
                                   id)]);
                }
                break;
            }
            case lispb::schema::TypeDefinitionKind::static_table: {
                auto const source{
                    graph_.definitions_.static_tables
                        .source[graph_.definitions_.static_tables.index(graph_.definition(type))]};
                for (auto const column : store_.items(
                         store_.static_tables.columns[store_.static_tables.index(source)])) {
                    follow(graph_.resolved(graph_.module(type),
                                           store_.static_table_columns
                                               .type[store_.static_table_columns.index(column)]));
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

    void build_edges() {
        std::vector<std::pair<TypeId, TypeId>> edges;
        auto const count{graph_.types().size()};
        auto& definitions{graph_.definitions_};
        for (std::uint32_t index{}; index < count; ++index) {
            auto const owner{TypeId{index}};
            auto add = [&](TypeId const dependency) {
                assert(lispb::schema::is_valid(dependency));
                if (dependency != owner) {
                    edges.emplace_back(owner, dependency);
                }
            };
            switch ((graph_.definitions_ids_[index]).kind()) {
                case lispb::schema::TypeDefinitionKind::enumeration: {
                    if (is_valid(definitions.enums.underlying_type[definitions.enums.index(
                            graph_.definitions_ids_[index])])) {
                        add(graph_.occurrences().type[graph_.occurrences().index(
                            definitions.enums.underlying_type[definitions.enums.index(
                                graph_.definitions_ids_[index])])]);
                    }
                    break;
                }
                case lispb::schema::TypeDefinitionKind::static_table: {
                    auto const source{
                        definitions.static_tables
                            .source[definitions.static_tables.index(graph_.definition(owner))]};
                    auto const module{graph_.module(owner)};
                    for (auto const column : store_.items(
                             store_.static_tables.columns[store_.static_tables.index(source)])) {
                        auto const occurrence{
                            graph_.resolved(module,
                                            store_.static_table_columns
                                                .type[store_.static_table_columns.index(column)])};
                        add(graph_.occurrences().type[graph_.occurrences().index(occurrence)]);
                    }
                    for (auto const group : store_.items(
                             store_.static_tables.groups[store_.static_tables.index(source)])) {
                        auto const occurrence{
                            graph_.resolved(module,
                                            store_.static_table_groups
                                                .type[store_.static_table_groups.index(group)])};
                        add(graph_.occurrences().type[graph_.occurrences().index(occurrence)]);
                    }
                    break;
                }
                case lispb::schema::TypeDefinitionKind::facade: {
                    auto const source{
                        definitions.facades
                            .source[definitions.facades.index(graph_.definition(owner))]};
                    auto const module{graph_.module(owner)};
                    auto add_reference = [&](codegen::TypeRefId reference) {
                        auto const occurrence{graph_.resolved(module, reference)};
                        add(graph_.occurrences().type[graph_.occurrences().index(occurrence)]);
                    };
                    add_reference(store_.facades.target_type[store_.facades.index(source)]);
                    for (auto const method :
                         store_.items(store_.facades.methods[store_.facades.index(source)])) {
                        add_reference(
                            store_.facade_methods.return_type[store_.facade_methods.index(method)]);
                        for (auto const parameter :
                             store_.items(store_.facade_methods
                                              .parameters[store_.facade_methods.index(method)])) {
                            add_reference(
                                store_.parameters.type[store_.parameters.index(parameter)]);
                        }
                    }
                    break;
                }
                case lispb::schema::TypeDefinitionKind::homogeneous_storage: {
                    add(graph_.occurrences().type[graph_.occurrences().index(
                        definitions.homogeneous_storage
                            .value_type[definitions.homogeneous_storage.index(
                                graph_.definitions_ids_[index])])]);
                    if (is_valid(definitions.homogeneous_storage
                                     .equivalent_type[definitions.homogeneous_storage.index(
                                         graph_.definitions_ids_[index])])) {
                        add(graph_.occurrences().type[graph_.occurrences().index(
                            definitions.homogeneous_storage
                                .equivalent_type[definitions.homogeneous_storage.index(
                                    graph_.definitions_ids_[index])])]);
                    }
                    for (auto const input : definitions.homogeneous_storage.input_types_of(
                             graph_.definitions_ids_[index])) {
                        add(graph_.occurrences().type[graph_.occurrences().index(input)]);
                    }
                    break;
                }
                case lispb::schema::TypeDefinitionKind::integer_scalar: {
                    if (is_valid(definitions.integer_scalars
                                     .cpp_representation[definitions.integer_scalars.index(
                                         graph_.definitions_ids_[index])])) {
                        add(graph_.occurrences().type[graph_.occurrences().index(
                            definitions.integer_scalars
                                .cpp_representation[definitions.integer_scalars.index(
                                    graph_.definitions_ids_[index])])]);
                    }
                    if (is_valid(definitions.integer_scalars
                                     .relationship[definitions.integer_scalars.index(
                                         graph_.definitions_ids_[index])])) {
                        add(graph_.occurrences().type[graph_.occurrences().index(
                            graph_.relationships().target[graph_.relationships().index(
                                definitions.integer_scalars
                                    .relationship[definitions.integer_scalars.index(
                                        graph_.definitions_ids_[index])])])]);
                    }
                    break;
                }
                case lispb::schema::TypeDefinitionKind::linear_quantized: {
                    add(graph_.occurrences().type[graph_.occurrences().index(
                        definitions.linear_quantized.source[definitions.linear_quantized.index(
                            graph_.definitions_ids_[index])])]);
                    break;
                }
                case lispb::schema::TypeDefinitionKind::integer_varint: {
                    add(graph_.occurrences().type[graph_.occurrences().index(
                        definitions.integer_varints.source[definitions.integer_varints.index(
                            graph_.definitions_ids_[index])])]);
                    break;
                }
                case lispb::schema::TypeDefinitionKind::optional_sentinel: {
                    add(graph_.occurrences().type[graph_.occurrences().index(
                        definitions.optional_sentinels.source[definitions.optional_sentinels.index(
                            graph_.definitions_ids_[index])])]);
                    break;
                }
                case lispb::schema::TypeDefinitionKind::optional_presence_bit: {
                    add(graph_.occurrences().type[graph_.occurrences().index(
                        definitions.optional_presence_bits
                            .source[definitions.optional_presence_bits.index(
                                graph_.definitions_ids_[index])])]);
                    break;
                }
                case lispb::schema::TypeDefinitionKind::packed: {
                    add(graph_.occurrences().type[graph_.occurrences().index(
                        definitions.packed.storage_type[definitions.packed.index(
                            graph_.definitions_ids_[index])])]);
                    for (auto const id :
                         definitions.packed.segments_of(graph_.definitions_ids_[index])) {
                        if (auto const field{((id).kind() == lispb::schema::PackedSegmentKind::field
                                                  ? std::optional{id}
                                                  : std::nullopt)}) {
                            add(graph_.occurrences().type[graph_.occurrences().index(
                                definitions.packed_fields
                                    .semantic_type[definitions.packed_fields.index(*field)])]);
                            if (is_valid(
                                    definitions.packed_fields
                                        .relationship[definitions.packed_fields.index(*field)])) {
                                add(graph_.occurrences().type[graph_.occurrences().index(
                                    graph_.relationships().target[graph_.relationships().index(
                                        definitions.packed_fields.relationship
                                            [definitions.packed_fields.index(*field)])])]);
                            }
                        }
                    }
                    break;
                }
                case lispb::schema::TypeDefinitionKind::record: {
                    for (auto const id :
                         definitions.records.members_of(graph_.definitions_ids_[index])) {
                        auto const member{id};
                        add(graph_.occurrences().type[graph_.occurrences().index(
                            definitions.record_members
                                .semantic_type[definitions.record_members.index(member)])]);
                        if (is_valid(definitions.record_members
                                         .relationship[definitions.record_members.index(member)])) {
                            add(graph_.occurrences().type[graph_.occurrences().index(
                                graph_.relationships().target[graph_.relationships().index(
                                    definitions.record_members.relationship
                                        [definitions.record_members.index(member)])])]);
                        }
                    }
                    break;
                }
                case lispb::schema::TypeDefinitionKind::union_type: {
                    for (auto const id :
                         definitions.unions.alternatives_of(graph_.definitions_ids_[index])) {
                        add(graph_.occurrences().type[graph_.occurrences().index(
                            definitions.union_alternatives
                                .semantic_type[definitions.union_alternatives.index(id)])]);
                    }
                    break;
                }
                case lispb::schema::TypeDefinitionKind::tagged_union: {
                    add(graph_.occurrences().type[graph_.occurrences().index(
                        definitions.tagged_unions.discriminant[definitions.tagged_unions.index(
                            graph_.definitions_ids_[index])])]);
                    for (auto const id : definitions.tagged_unions.alternatives_of(
                             graph_.definitions_ids_[index])) {
                        add(graph_.occurrences().type[graph_.occurrences().index(
                            definitions.tagged_union_alternatives
                                .semantic_type[definitions.tagged_union_alternatives.index(id)])]);
                    }
                    break;
                }
                case lispb::schema::TypeDefinitionKind::soa: {
                    if (is_valid(definitions.soas.equivalent_type[definitions.soas.index(
                            graph_.definitions_ids_[index])])) {
                        add(graph_.occurrences().type[graph_.occurrences().index(
                            definitions.soas.equivalent_type[definitions.soas.index(
                                graph_.definitions_ids_[index])])]);
                    }
                    for (auto const id :
                         definitions.soas.columns_of(graph_.definitions_ids_[index])) {
                        auto const column{id};
                        add(graph_.occurrences().type[graph_.occurrences().index(
                            definitions.soa_columns
                                .semantic_type[definitions.soa_columns.index(column)])]);
                        if (definitions.soa_columns
                                .nested_type[definitions.soa_columns.index(column)]) {
                            add(*definitions.soa_columns
                                     .nested_type[definitions.soa_columns.index(column)]);
                        }
                        if (is_valid(definitions.soa_columns
                                         .relationship[definitions.soa_columns.index(column)])) {
                            add(graph_.occurrences().type[graph_.occurrences().index(
                                graph_.relationships().target[graph_.relationships().index(
                                    definitions.soa_columns
                                        .relationship[definitions.soa_columns.index(column)])])]);
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
        for (auto const use : graph_.type_uses()) {
            if (graph_.owner(use) &&
                graph_.role(use).category == codegen::TypeReferenceCategory::function &&
                ((graph_.definition(*graph_.owner(use))).kind() ==
                 lispb::schema::TypeDefinitionKind::record) &&
                graph_.occurrences().type[graph_.occurrences().index(graph_.target(use))] !=
                    *graph_.owner(use)) {
                edges.emplace_back(
                    *graph_.owner(use),
                    graph_.occurrences().type[graph_.occurrences().index(graph_.target(use))]);
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
    std::vector<std::optional<codegen::HomogeneousValueId>> homogeneous_values_;
    std::map<codegen::ModuleHandle, StringId> module_headers_;
    std::map<IdentifierId, codegen::ModuleHandle> modules_by_name_;
    std::multimap<IdentifierId, TypeId> declarations_by_name_;
    std::map<IdentifierId, TypeId> raw_external_types_;
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
auto TypeGraph::module_name(TypeId const id) const -> IdentifierId {
    return module_names_[index(id)];
}
auto TypeGraph::namespace_name(TypeId const id) const -> IdentifierId {
    return namespace_names_[index(id)];
}
auto TypeGraph::name(TypeId const id) const -> IdentifierId {
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
auto TypeGraph::find_declared(codegen::ModuleHandle const module, IdentifierId const name) const
    -> std::optional<TypeId> {
    auto const found{declarations_by_module_name_.find({module, name})};
    return found == declarations_by_module_name_.end() ? std::nullopt
                                                       : std::optional{found->second};
}
auto TypeGraph::find_registered(IdentifierId const name) const -> std::optional<TypeId> {
    auto const found{registered_types_.find(name)};
    return found == registered_types_.end() ? std::nullopt : std::optional{found->second};
}
auto TypeGraph::find_reference(codegen::TypeRefId const reference_id,
                               codegen::ModuleHandle const module) const -> std::optional<TypeId> {
    auto const reference{reference_id};
    return find_named(declarations().type_refs.name[declarations().type_refs.index(reference)],
                      module);
}

auto TypeGraph::find_named(IdentifierId const identifier, codegen::ModuleHandle const module) const
    -> std::optional<TypeId> {
    auto const name{strings().view(identifier)};
    if (name.starts_with('@')) {
        return find_registered(strings().find(name.substr(1)));
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
