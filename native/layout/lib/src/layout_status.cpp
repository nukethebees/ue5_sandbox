#include <ioj/layout/layout_status.hpp>

#include <ioj/layout/analyzer/analyzer.hpp>
#include <ioj/layout/analyzer/enum_domain_analysis.hpp>
#include <ioj/layout/analyzer/fixed_point_analysis.hpp>
#include <ioj/layout/analyzer/integer_scalar_analysis.hpp>
#include <ioj/layout/analyzer/integer_varint_analysis.hpp>
#include <ioj/layout/analyzer/mini_float_analysis.hpp>
#include <ioj/layout/analyzer/optional_presence_bit_analysis.hpp>
#include <ioj/layout/analyzer/optional_sentinel_analysis.hpp>
#include <ioj/layout/analyzer/packed_analysis.hpp>
#include <ioj/layout/analyzer/record_analysis.hpp>
#include <ioj/layout/analyzer/soa_analysis.hpp>
#include <ioj/layout/analyzer/tagged_union_analysis.hpp>
#include <ioj/layout/analyzer/union_analysis.hpp>
#include <ioj/layout/declaration_capabilities.hpp>
#include <ioj/layout/physical_facts.hpp>

#include <codegen/schema/soa_backend.h>
#include <lispb/schema/soa_types.h>

#include <algorithm>

namespace ioj::layout {
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
}
