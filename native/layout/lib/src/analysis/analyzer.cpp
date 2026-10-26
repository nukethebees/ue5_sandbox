#include <ioj/layout/analysis/analyzer.hpp>

#include "../analyzer/aggregate_pass.hpp"
#include "../analyzer/packed_pass.hpp"
#include "../analyzer/physical_pass.hpp"
#include "../analyzer/scalar_pass.hpp"
#include "../analyzer/soa_pass.hpp"
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analysis/analysis_context.hpp>
#include <ioj/layout/analysis/analysis_distributions.hpp>
#include <ioj/layout/analysis/analysis_request.hpp>
#include <ioj/layout/analysis/analysis_storage.hpp>
#include <ioj/layout/analysis/analysis_totals.hpp>
#include <ioj/layout/analysis/analysis_workloads.hpp>
#include <ioj/layout/document.hpp>
#include <ioj/layout/editor_session.hpp>
#include <ioj/layout/resolved_variant.hpp>
#include <ioj/layout/variants.hpp>

#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cassert>
#include <vector>

namespace ioj::layout {
namespace analysis_detail {

auto same_abi(AbiProfile const& first, AbiProfile const& second) -> bool {
    return &first == &second || first == second;
}

auto same_scenario(AnalysisRequest const& first, AnalysisRequest const& second) -> bool {
    return first.variant == second.variant && first.default_capacity == second.default_capacity &&
           same_abi(*first.abi, *second.abi);
}

auto same_workload(AnalysisRequest const& first, AnalysisRequest const& second) -> bool {
    if (first.element_count != second.element_count || first.multiplicity != second.multiplicity ||
        first.distribution_requested != second.distribution_requested) {
        return false;
    }
    if ((first.accesses == nullptr) != (second.accesses == nullptr) ||
        (first.accesses && *first.accesses != *second.accesses)) {
        return false;
    }
    return std::ranges::equal(first.varint_distribution, second.varint_distribution) &&
           std::ranges::equal(first.union_distribution, second.union_distribution);
}

auto resolve_variant(Document const& document, VariantId id) -> ResolvedVariant {
    auto const& authored{document.variants()};
    assert(authored.contains(id));
    ResolvedVariant result;
    result.id = id;
    result.name = authored.name(id);
    auto const& settings{authored.settings(id)};
    auto const storage_count{settings.packed_types.size()};
    for (std::size_t row{}; row < storage_count; ++row) {
        if (auto const type{document.declaration_type(settings.packed_types[row])}) {
            result.overrides.packed_storage_types.emplace(*type, settings.packed_storage[row]);
        }
    }
    auto const field_count{settings.field_types.size()};
    for (std::size_t row{}; row < field_count; ++row) {
        if (auto const type{document.declaration_type(settings.field_types[row])}) {
            result.overrides.packed_field_widths.emplace(
                FieldOverrideId{*type, settings.field_names[row]}, settings.field_widths[row]);
        }
    }
    auto const column_count{settings.column_types.size()};
    for (std::size_t row{}; row < column_count; ++row) {
        if (auto const type{document.declaration_type(settings.column_types[row])}) {
            result.overrides.soa_column_types.emplace(
                FieldOverrideId{*type, settings.column_names[row]}, settings.column_spelling[row]);
        }
    }
    auto const capacity_count{settings.capacity_types.size()};
    for (std::size_t row{}; row < capacity_count; ++row) {
        if (auto const type{document.declaration_type(settings.capacity_types[row])}) {
            result.overrides.capacities.emplace(*type, settings.capacities[row]);
        }
    }
    return result;
}

void validate_memory(AnalysisContext& context, AbiProfile const& abi) {
    auto const& memory{abi.memory_facts()};
    if (!memory.cache_line_bytes) {
        context.report({DiagnosticSeverity::warning,
                        "Cache-line size is unknown for ABI profile '" + abi.name() + "'."});
    } else if (*memory.cache_line_bytes == 0) {
        context.report(
            {DiagnosticSeverity::error, "ABI profile cache-line size must be non-zero."});
    }
    if (!memory.page_bytes) {
        context.report({DiagnosticSeverity::warning,
                        "Page size is unknown for ABI profile '" + abi.name() + "'."});
    } else if (*memory.page_bytes == 0) {
        context.report({DiagnosticSeverity::error, "ABI profile page size must be non-zero."});
    }
}
}

void Analyzer::analyze(AnalysisContext& context, std::span<AnalysisRequest const> requests) {
    using namespace analysis_detail;
    auto& storage{context.storage};
    storage.clear();
    context.document.session().begin_operation(context.document.id());
    if (requests.empty()) {
        return;
    }
    assert(context.document.valid());
    analyze_scalar_domains(context);
    std::vector<std::size_t> scenarios;
    std::vector<ResolvedVariant> variants;
    std::vector<AbiProfile const*> validated_profiles;
    auto const request_count{requests.size()};
    for (std::size_t request_row{}; request_row < request_count; ++request_row) {
        auto const& request{requests[request_row]};
        assert(request.abi);
        assert(lispb::schema::is_valid(request.type));
        auto const found{std::ranges::find_if(
            scenarios, [&](auto row) { return same_scenario(requests[row], request); })};
        auto const scenario{ScenarioId{static_cast<std::uint32_t>(found - scenarios.begin())}};
        if (found == scenarios.end()) {
            scenarios.push_back(request_row);
            if (std::ranges::none_of(validated_profiles, [&](auto const* abi) {
                    return same_abi(*abi, *request.abi);
                })) {
                validate_memory(context, *request.abi);
                validated_profiles.push_back(request.abi);
            }
            auto variant_row{std::ranges::find(variants, request.variant, &ResolvedVariant::id)};
            if (variant_row == variants.end()) {
                variants.push_back(resolve_variant(context.document, request.variant));
                variant_row = variants.end() - 1;
            }
            auto const& variant{*variant_row};
            for (auto const type : context.types.types()) {
                storage.types.append(type, scenario);
            }
            analyze_physical_layouts(context, scenario, request, variant);
            for (auto const type : context.types.types()) {
                auto const id{storage.types.find(type, scenario)};
                auto const kind{context.types.definition(type).kind()};
                auto const definition{context.types.definition(type)};
                auto const& definitions{context.types.definitions()};
                auto const packed{
                    kind == lispb::schema::TypeDefinitionKind::record
                        ? definitions.records.layout[definitions.records.index(definition)] ==
                              lispb::codegen::LayoutKind::packed
                        : kind == lispb::schema::TypeDefinitionKind::union_type &&
                              definitions.unions.layout[definitions.unions.index(definition)] ==
                                  lispb::codegen::LayoutKind::packed};
                if (packed) {
                    analyze_packed_layout(context, id, variant);
                } else if (kind == lispb::schema::TypeDefinitionKind::enumeration &&
                           !context.types.strings()
                                .view(context.types.cpp_spelling(type))
                                .empty()) {
                    analyze_enum_backing(context, id);
                }
            }
            for (auto const type : context.types.types()) {
                if (context.types.definition(type).kind() ==
                    lispb::schema::TypeDefinitionKind::soa) {
                    analyze_soa_layout(
                        context, storage.types.find(type, scenario), request, variant);
                }
            }
            analyze_relationships(context, scenario);
        }
        auto const id{storage.types.find(request.type, scenario)};
        std::optional<std::size_t> reused;
        for (std::size_t prior{}; prior < request_row; ++prior) {
            if (storage.runs[prior].analysis == id && same_workload(requests[prior], request)) {
                reused = prior;
                break;
            }
        }
        if (reused) {
            storage.runs.push_back(storage.runs[*reused]);
            storage.totals.push_back(storage.totals[*reused]);
            continue;
        }
        AnalysisRun run{.analysis = id};
        storage.totals.push_back(compute_element_totals(context, request, id));
        [[maybe_unused]] auto const kind{context.types.definition(request.type).kind()};
        if (request.accesses) {
            assert(kind == lispb::schema::TypeDefinitionKind::record ||
                   kind == lispb::schema::TypeDefinitionKind::union_type ||
                   kind == lispb::schema::TypeDefinitionKind::soa);
            run.access = analyze_access(context, request, id);
        }
        if (request.distribution_requested || !request.varint_distribution.empty() ||
            !request.union_distribution.empty()) {
            assert(kind == lispb::schema::TypeDefinitionKind::integer_varint ||
                   kind == lispb::schema::TypeDefinitionKind::union_type);
            run.distribution = analyze_distribution(context, request, id);
        }
        storage.runs.push_back(run);
    }
}
}
