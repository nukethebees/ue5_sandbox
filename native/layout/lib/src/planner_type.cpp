#include <ioj/layout/planner_type.hpp>

#include <ioj/layout/analyzer.hpp>

#include <codegen/schema.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/soa_backend.h>
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
#include <lispb/schema/packed_types.h>
#include <lispb/schema/record_types.h>
#include <lispb/schema/soa_source_kind.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/static_table_types.h>
#include <lispb/schema/tagged_union_types.h>
#include <lispb/schema/type_definitions.h>
#include <lispb/schema/type_graph.h>
#include <lispb/schema/type_identity.h>
#include <lispb/schema/type_origin.h>
#include <lispb/schema/union_types.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <ranges>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace ioj::layout {

auto external_dependencies(lispb::schema::TypeGraph const& types, AbiProfile const* const abi)
    -> std::vector<ExternalDependency> {
    std::map<std::string, ExternalDependency> grouped;
    auto append_unique = [](auto& values, auto const& value) {
        if (std::ranges::find(values, value) == values.end()) {
            values.push_back(value);
        }
    };
    for (std::size_t index{}; index < types.types().size(); ++index) {
        auto const& node{types.types()[index]};
        auto const external{
            ((types.definition(node)).kind() == lispb::schema::TypeDefinitionKind::external
                 ? std::optional{types.definition(node)}
                 : std::nullopt)};
        if (!external) {
            continue;
        }
        if (std::string{types.strings().view(types.cpp_spelling(node))} == "void" ||
            std::string{types.strings().view(types.cpp_spelling(node))} == "auto" ||
            std::string{types.strings().view(types.cpp_spelling(node))} == "decltype(auto)") {
            continue;
        }
        auto& entry{grouped[lispb::codegen::native_spelling(
            std::string{types.strings().view(types.cpp_spelling(node))})]};
        entry.cpp_spelling = lispb::codegen::native_spelling(
            std::string{types.strings().view(types.cpp_spelling(node))});
        entry.types.push_back(lispb::schema::TypeId{static_cast<std::uint32_t>(index)});
        if (abi != nullptr) {
            entry.physical_facts =
                PhysicalFactsResolver{types, *abi}.resolve(entry.types.back()).facts;
        }
        entry.semantics_known =
            entry.semantics_known ||
            (types.definitions()
                 .externals.integer_semantics[types.definitions().externals.index(*external)]
                 .has_value() ||
             types.definitions()
                 .externals.floating_semantics[types.definitions().externals.index(*external)]
                 .has_value());
        for (auto const& name : types.definitions().externals.registered_names_of(*external)) {
            append_unique(entry.registered_names, std::string{types.strings().view(name)});
        }
    }
    for (std::size_t index{}; index < types.type_uses().size(); ++index) {
        auto const& use{types.type_uses()[index]};
        auto const& node{types.occurrences().type[types.occurrences().index(types.target(use))]};
        if (!((types.definition(node)).kind() == lispb::schema::TypeDefinitionKind::external)) {
            continue;
        }
        auto const found{grouped.find(lispb::codegen::native_spelling(
            std::string{types.strings().view(types.cpp_spelling(node))}))};
        if (found == grouped.end()) {
            continue;
        }
        auto& entry{found->second};
        entry.uses.push_back(use);
        append_unique(
            entry.modules,
            std::string{types.strings().view(
                types.declarations()
                    .module_settings_store.name[types.declarations().module_settings_store.index(
                        types.declarations().module_settings(types.module(use)))])});
        if (types.declaration(use).value != 0) {
            auto const settings{types.declarations().module_settings(types.module(use))};
            append_unique(
                entry.declarations,
                lispb::schema::TypeIdentity{
                    .origin = lispb::schema::TypeOrigin::declaration,
                    .module_name =
                        types.declarations()
                            .module_settings_store
                            .name[types.declarations().module_settings_store.index(settings)],
                    .namespace_name =
                        types.declarations()
                            .module_settings_store
                            .namespace_name[types.declarations().module_settings_store.index(
                                settings)]
                            .value_or(lispb::IdentifierId{}),
                    .name = lispb::codegen::declaration_name(types.declarations(),
                                                             types.declaration(use))});
        }
    }
    std::vector<ExternalDependency> result;
    for (auto& [spelling, entry] : grouped) {
        result.push_back(std::move(entry));
    }
    std::ranges::stable_sort(result, [](auto const& left, auto const& right) {
        return left.declarations.size() > right.declarations.size();
    });
    return result;
}

auto analyze_external_type(lispb::schema::TypeGraph const& types,
                           lispb::schema::TypeId const type,
                           ResolvedVariant const& variant,
                           AbiProfile const& abi,
                           std::uint64_t const capacity,
                           SoaAllocationStrategy const allocation_strategy)
    -> ExternalTypeAnalysis {
    auto const& node{type};
    auto const& external{types.definition(node)};
    PhysicalFactsResolver resolver{types, abi, &variant};
    ExternalTypeAnalysis result{
        .semantics_known =
            (types.definitions()
                 .externals.integer_semantics[types.definitions().externals.index(external)]
                 .has_value() ||
             types.definitions()
                 .externals.floating_semantics[types.definitions().externals.index(external)]
                 .has_value()),
        .physical = resolver.resolve(type),
        .blocked_declarations = {}};
    if (result.physical.facts.has_value()) {
        return result;
    }
    auto const spelling{lispb::codegen::native_spelling(
        std::string{types.strings().view(types.cpp_spelling(node))})};
    auto const count{types.types().size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const id{lispb::schema::TypeId{static_cast<std::uint32_t>(index)}};
        auto const& candidate{id};
        if (types.origin(candidate) != lispb::schema::TypeOrigin::declaration) {
            continue;
        }
        std::vector<Diagnostic> diagnostics;
        if (auto const soa{
                ((types.definition(candidate)).kind() == lispb::schema::TypeDefinitionKind::soa
                     ? std::optional{types.definition(candidate)}
                     : std::nullopt)};
            static_cast<bool>(soa) &&
            types.definitions().soas.backend[types.definitions().soas.index(*soa)] ==
                lispb::codegen::SoaBackend::standard_library) {
            diagnostics =
                Analyzer::analyze_soa(types, id, variant, abi, capacity, allocation_strategy)
                    .diagnostics;
        } else {
            diagnostics = resolver.resolve(id).diagnostics;
        }
        if (std::ranges::any_of(diagnostics, [&](auto const& diagnostic) {
                return diagnostic.missing_physical_type == spelling;
            })) {
            auto owner{types.identity(candidate)};
            owner.name = lispb::codegen::declaration_name(types.declarations(),
                                                          types.source_declaration(candidate));
            if (std::ranges::find(result.blocked_declarations, owner) ==
                result.blocked_declarations.end()) {
                result.blocked_declarations.push_back(owner);
            }
        }
    }
    return result;
}

namespace {

auto has_error(std::vector<Diagnostic> const& diagnostics) -> bool {
    return std::ranges::any_of(diagnostics, [](auto const& diagnostic) {
        return diagnostic.severity == DiagnosticSeverity::error;
    });
}

auto status(std::vector<Diagnostic> const& diagnostics, bool const facts_available)
    -> LayoutStatus {
    if (has_error(diagnostics)) {
        return LayoutStatus::error;
    }
    return facts_available ? LayoutStatus::available : LayoutStatus::unknown;
}

} // namespace

auto integer_source_reference(lispb::schema::TypeGraph const& types,
                              lispb::schema::TypeId const node) -> std::string {
    return types.origin(node) == lispb::schema::TypeOrigin::registered_external
             ? "@" + std::string{types.strings().view(types.name(node))}
             : std::string{types.strings().view(types.cpp_spelling(node))};
}

auto declaration_capabilities(lispb::schema::TypeGraph const& types,
                              lispb::schema::TypeId const node) -> DeclarationCapabilities {
    switch ((types.definition(node)).kind()) {
        case lispb::schema::TypeDefinitionKind::external: {
            return {.kind = DeclarationKind::external, .inspectable = true};
        }
        case lispb::schema::TypeDefinitionKind::enumeration: {
            return {.kind = DeclarationKind::enumeration,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::integer_scalar: {
            return {.kind = DeclarationKind::integer_scalar,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::linear_quantized: {
            return {.kind = DeclarationKind::linear_quantized,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::integer_varint: {
            return {.kind = DeclarationKind::integer_varint,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::fixed_point: {
            return {.kind = DeclarationKind::fixed_point,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::mini_float: {
            return {.kind = DeclarationKind::mini_float,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::optional_sentinel: {
            return {.kind = DeclarationKind::optional_sentinel,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::optional_presence_bit: {
            return {.kind = DeclarationKind::optional_presence_bit,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::packed: {
            return {.kind = DeclarationKind::packed,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true,
                    .supports_variant_overrides = true};
        }
        case lispb::schema::TypeDefinitionKind::record: {
            return {.kind = DeclarationKind::record,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::union_type: {
            return {.kind = DeclarationKind::union_,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::tagged_union: {
            return {.kind = DeclarationKind::tagged_union,
                    .inspectable = true,
                    .editable = true,
                    .physical_analysis_available = true};
        }
        case lispb::schema::TypeDefinitionKind::static_table: {
            return {.kind = DeclarationKind::static_table, .inspectable = true, .editable = true};
        }
        case lispb::schema::TypeDefinitionKind::facade: {
            return {.kind = DeclarationKind::facade, .inspectable = true, .editable = true};
        }
        case lispb::schema::TypeDefinitionKind::homogeneous_storage: {
            return {
                .kind = DeclarationKind::homogeneous_layout, .inspectable = true, .editable = true};
        }
        case lispb::schema::TypeDefinitionKind::soa: {
            auto const supported{
                types.definitions()
                    .soas.backend[types.definitions().soas.index(types.definition(node))] ==
                lispb::codegen::SoaBackend::standard_library};
            auto const vector{types.definitions().soas.source_kind[types.definitions().soas.index(
                                  types.definition(node))] == lispb::schema::SoaSourceKind::vector};
            return {.kind = vector ? DeclarationKind::vector_soa : DeclarationKind::soa,
                    .inspectable = true,
                    .editable = supported && !vector,
                    .physical_analysis_available = supported,
                    .supports_variant_overrides = supported};
        }
        default:
            std::unreachable();
    }
}

auto declaration_capabilities(lispb::codegen::DeclarationHandle const declaration)
    -> DeclarationCapabilities {
    switch (declaration.kind()) {
        case lispb::codegen::DeclarationKind::homogeneous_layout:
            return {
                .kind = DeclarationKind::homogeneous_layout, .inspectable = true, .editable = true};
        case lispb::codegen::DeclarationKind::static_table:
            return {.kind = DeclarationKind::static_table, .inspectable = true, .editable = true};
        case lispb::codegen::DeclarationKind::facade:
            return {.kind = DeclarationKind::facade, .inspectable = true, .editable = true};
        default:
            return {};
    }
}

auto declaration_kind_label(DeclarationKind const kind) -> char const* {
    switch (kind) {
        case DeclarationKind::external:
            return "external";
        case DeclarationKind::enumeration:
            return "enum";
        case DeclarationKind::integer_scalar:
            return "integer scalar";
        case DeclarationKind::linear_quantized:
            return "linear quantized";
        case DeclarationKind::integer_varint:
            return "integer varint";
        case DeclarationKind::fixed_point:
            return "fixed point";
        case DeclarationKind::mini_float:
            return "mini float";
        case DeclarationKind::optional_sentinel:
            return "optional sentinel";
        case DeclarationKind::optional_presence_bit:
            return "optional presence bit";
        case DeclarationKind::packed:
            return "packed value";
        case DeclarationKind::record:
            return "record";
        case DeclarationKind::union_:
            return "union";
        case DeclarationKind::tagged_union:
            return "tagged union";
        case DeclarationKind::soa:
            return "SoA";
        case DeclarationKind::vector_soa:
            return "vector-soa";
        case DeclarationKind::homogeneous_layout:
            return "layout";
        case DeclarationKind::static_table:
            return "table";
        case DeclarationKind::facade:
            return "facade";
    }
    return "unknown";
}

auto declaration_status(lispb::schema::TypeGraph const& types,
                        lispb::schema::TypeId const type,
                        ResolvedVariant const& variant,
                        AbiProfile const& abi,
                        std::uint64_t const default_capacity) -> LayoutStatus {
    auto const targets{
        Analyzer::derive_relationship_target_facts(types, variant, abi, default_capacity)};
    return declaration_status(types,
                              type,
                              variant,
                              abi,
                              default_capacity,
                              1,
                              SoaAllocationStrategy::separate_columns,
                              targets);
}

auto declaration_status(lispb::schema::TypeGraph const& types,
                        lispb::schema::TypeId const type,
                        ResolvedVariant const& variant,
                        AbiProfile const& abi,
                        std::uint64_t const default_capacity,
                        std::uint64_t const element_count,
                        SoaAllocationStrategy const allocation_strategy,
                        std::span<RelationshipTargetFacts const> relationship_targets)
    -> LayoutStatus {
    if (!lispb::schema::is_valid(type) || type.value >= types.types().size()) {
        return LayoutStatus::error;
    }
    auto const& definition{types.definition(type)};
    if (((definition).kind() == lispb::schema::TypeDefinitionKind::external)) {
        auto const physical{PhysicalFactsResolver{types, abi, &variant}.resolve(type)};
        return status(physical.diagnostics, physical.facts.has_value());
    }
    if (!declaration_capabilities(types, type).physical_analysis_available) {
        return LayoutStatus::unknown;
    }
    if (((definition).kind() == lispb::schema::TypeDefinitionKind::enumeration)) {
        auto const analysis{Analyzer::analyze_enum(types, type, abi, element_count)};
        return status(analysis.diagnostics, analysis.backing_facts.has_value());
    }
    if (((definition).kind() == lispb::schema::TypeDefinitionKind::packed)) {
        auto const analysis{Analyzer::analyze_packed(
            types, type, variant, abi, element_count, relationship_targets)};
        return status(analysis.diagnostics, analysis.storage_facts.has_value());
    }
    if (((definition).kind() == lispb::schema::TypeDefinitionKind::record)) {
        auto const analysis{Analyzer::analyze_record(types, type, abi, element_count)};
        return status(analysis.diagnostics, analysis.size_bytes.has_value());
    }
    if (((definition).kind() == lispb::schema::TypeDefinitionKind::union_type)) {
        auto const analysis{Analyzer::analyze_union(types, type, abi, element_count)};
        return status(analysis.diagnostics, analysis.size_bytes.has_value());
    }
    if (((definition).kind() == lispb::schema::TypeDefinitionKind::tagged_union)) {
        auto const analysis{Analyzer::analyze_tagged_union(types, type, abi, element_count)};
        return status(analysis.diagnostics, analysis.size_bytes.has_value());
    }
    if (auto const soa{((definition).kind() == lispb::schema::TypeDefinitionKind::soa
                            ? std::optional{definition}
                            : std::nullopt)}) {
        if (types.definitions().soas.backend[types.definitions().soas.index(*soa)] !=
            lispb::codegen::SoaBackend::standard_library) {
            return LayoutStatus::unknown;
        }
        auto const analysis{Analyzer::analyze_soa(
            types, type, variant, abi, default_capacity, allocation_strategy)};
        return status(analysis.diagnostics, analysis.total_payload_bytes.has_value());
    }
    if (((definition).kind() == lispb::schema::TypeDefinitionKind::integer_scalar)) {
        return status(
            Analyzer::analyze_integer_scalar(types, type, relationship_targets).diagnostics, true);
    }
    if (((definition).kind() == lispb::schema::TypeDefinitionKind::linear_quantized)) {
        return LayoutStatus::available;
    }
    if (((definition).kind() == lispb::schema::TypeDefinitionKind::integer_varint)) {
        return status(Analyzer::analyze_integer_varint(types, type, element_count).diagnostics,
                      true);
    }
    if (((definition).kind() == lispb::schema::TypeDefinitionKind::fixed_point)) {
        return status(Analyzer::analyze_fixed_point(types, type, element_count).diagnostics, true);
    }
    if (((definition).kind() == lispb::schema::TypeDefinitionKind::mini_float)) {
        return status(Analyzer::analyze_mini_float(types, type, element_count).diagnostics, true);
    }
    if (((definition).kind() == lispb::schema::TypeDefinitionKind::optional_sentinel)) {
        return status(Analyzer::analyze_optional_sentinel(types, type, element_count).diagnostics,
                      true);
    }
    return status(Analyzer::analyze_optional_presence_bit(types, type, element_count).diagnostics,
                  true);
}

} // namespace ioj::layout
