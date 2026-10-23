#include "soa_pass.hpp"

#include "../checked_arithmetic.hpp"
#include "../memory_regions.hpp"
#include "../variant_projection.hpp"
#include "physical_pass.hpp"
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analysis/analysis_context.hpp>
#include <ioj/layout/analysis/analysis_request.hpp>
#include <ioj/layout/analysis/analysis_storage.hpp>
#include <ioj/layout/resolved_variant.hpp>

#include <codegen/ast/cpp_types.h>
#include <codegen/schema/physical_type_use.h>
#include <lispb/schema/soa_columns.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <algorithm>

namespace ioj::layout::analysis_detail {
void analyze_soa_layout(AnalysisContext& context,
                        AnalysisId id,
                        AnalysisRequest const& request,
                        ResolvedVariant const& variant) {
    auto const& types{context.types};
    auto const type{context.storage.types.schema_type[id.value]};
    auto const soa{types.definition(type)};
    auto const& schema{types.definitions().soa_columns};
    auto const& soas{types.definitions().soas};
    auto const& occurrences{types.occurrences()};
    auto const& abi{*request.abi};
    auto const scenario{context.storage.types.scenario[id.value]};
    auto& columns{context.storage.columns};
    SoaLayout result;
    result.columns.begin = columns.parent.size();
    result.capacity = request.default_capacity;
    if (auto const found = variant.overrides.capacities.find(type);
        found != variant.overrides.capacities.end()) {
        result.capacity = found->second;
        result.capacity_overridden = true;
    }
    result.allocation_strategy = request.allocation_strategy;
    auto const members{soas.columns_of(soa)};
    result.allocation_count = result.capacity == 0 || members.empty() ? 0
                            : request.allocation_strategy == SoaAllocationStrategy::separate_columns
                                ? members.size()
                                : 1;
    std::optional<std::uint64_t> row_bytes{0};
    std::optional<std::uint64_t> total_bytes{0};
    std::optional<std::uint64_t> allocation_offset{0};
    std::optional<std::uint64_t> allocation_padding{0};
    std::optional<std::uint64_t> allocation_alignment{1};
    auto const cache{abi.memory_facts().cache_line_bytes};
    auto const page{abi.memory_facts().page_bytes};
    for (auto const column : members) {
        auto const schema_row{schema.index(column)};
        auto const use{schema.semantic_type[schema_row]};
        auto const occurrence_row{occurrences.index(use)};
        auto const row{columns.append()};
        columns.parent[row] = id;
        columns.name[row] = types.strings().view(schema.name[schema_row]);
        columns.semantic_type[row] = occurrences.type[occurrence_row];
        columns.schema_type[row] = context.storage.strings.intern(
            types.cpp_types().spelling(occurrences.cpp_type[occurrence_row]));
        auto const physical_type{effective_column_type(types, type, column, variant)};
        columns.physical_type[row] = context.storage.strings.intern(physical_type);
        columns.overridden[row] =
            static_cast<std::uint8_t>(variant.overrides.soa_column_types.contains(
                FieldOverrideId{type, schema.name[schema_row]}));
        auto const cv_qualified{
            columns.overridden[row]
                ? lispb::codegen::classify_physical_type_use(
                      types.cpp_types(), types.cpp_types().intern(physical_type))
                      .cv_qualified
                : occurrences.cv_qualified[occurrence_row] != 0};
        if (schema.kind[schema_row] != lispb::codegen::SoaMemberKind::array ||
            soas.backend[soas.index(soa)] != lispb::codegen::SoaBackend::standard_library ||
            cv_qualified) {
            context.report({DiagnosticSeverity::warning,
                            "SoA column '" + std::string{columns.name[row]} +
                                "' has an unsupported owning-storage representation."});
        } else {
            columns.type_facts[row] =
                columns.overridden[row]
                    ? physical_spelling(context, scenario, abi, physical_type, types.module(type))
                    : physical_occurrence(context, scenario, abi, use);
        }
        if (!columns.type_facts[row]) {
            row_bytes.reset();
            total_bytes.reset();
            allocation_offset.reset();
            allocation_padding.reset();
            allocation_alignment.reset();
            continue;
        }
        auto const size{
            context.storage.physical_facts.size_bytes()[columns.type_facts[row]->value]};
        auto const alignment{
            context.storage.physical_facts.alignment_bytes()[columns.type_facts[row]->value]};
        columns.total_bytes[row] = checked_multiply(size, result.capacity);
        if (!columns.total_bytes[row]) {
            context.report({DiagnosticSeverity::error,
                            "Byte count overflows uint64 for SoA column '" +
                                std::string{columns.name[row]} + "'."});
        }
        if (cache && *cache != 0) {
            columns.cache_line_tiling[row] = cache_line_tiling(size, *cache);
            columns.elements_per_cache_line[row] =
                columns.cache_line_tiling[row]->exact_elements_per_cache_line;
            if (columns.total_bytes[row]) {
                columns.minimum_cache_lines[row] =
                    minimum_regions(*columns.total_bytes[row], *cache);
            }
        }
        if (page && *page != 0) {
            columns.complete_elements_per_page[row] = *page / size;
            if (columns.total_bytes[row]) {
                columns.minimum_pages[row] = minimum_regions(*columns.total_bytes[row], *page);
            }
        }
        auto const row_known{row_bytes.has_value()};
        row_bytes = row_bytes.and_then([&](auto value) { return checked_add(value, size); });
        if (row_known && !row_bytes) {
            context.report(
                {DiagnosticSeverity::error, "SoA bytes per logical element overflow uint64."});
        }
        auto const total_known{total_bytes.has_value()};
        total_bytes = total_bytes.and_then([&](auto value) {
            return columns.total_bytes[row].and_then(
                [&](auto extent) { return checked_add(value, extent); });
        });
        if (total_known && !total_bytes) {
            context.report({DiagnosticSeverity::error, "SoA total payload bytes overflow uint64."});
        }
        if (allocation_alignment) {
            allocation_alignment = std::max(*allocation_alignment, alignment);
        }
        if (request.allocation_strategy == SoaAllocationStrategy::aligned_contiguous) {
            auto const aligned{allocation_offset.and_then(
                [&](auto offset) { return align_up(offset, alignment); })};
            if (aligned) {
                columns.allocation_offset_bytes[row] = aligned;
                columns.padding_before_bytes[row] = *aligned - *allocation_offset;
                allocation_padding = allocation_padding.and_then([&](auto padding) {
                    return checked_add(padding, *columns.padding_before_bytes[row]);
                });
            } else {
                allocation_padding.reset();
            }
            auto const previous_known{allocation_offset.has_value()};
            allocation_offset = aligned.and_then([&](auto offset) {
                return columns.total_bytes[row].and_then(
                    [&](auto extent) { return checked_add(offset, extent); });
            });
            if (previous_known && !allocation_offset) {
                context.report({DiagnosticSeverity::error,
                                "Aligned contiguous SoA allocation arithmetic overflows."});
            }
        }
    }
    result.columns.count = columns.parent.size() - result.columns.begin;
    result.bytes_per_logical_element = row_bytes;
    result.total_payload_bytes = total_bytes;
    if (request.allocation_strategy == SoaAllocationStrategy::separate_columns) {
        result.total_allocation_bytes = total_bytes;
        result.total_alignment_padding_bytes = 0;
    } else {
        result.total_allocation_bytes = allocation_offset;
        result.total_alignment_padding_bytes = allocation_padding;
        result.allocation_alignment_bytes = allocation_alignment;
    }
    context.storage.types.soa[id.value] = result;
}
}
