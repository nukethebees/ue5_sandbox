#pragma once

#include <ioj/layout/analyzer/access_footprint_summary.hpp>
#include <ioj/layout/analyzer/access_intent.hpp>
#include <ioj/layout/analyzer/cache_capacity_analysis.hpp>
#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/soa_allocation_strategy.hpp>
#include <ioj/layout/analyzer/soa_column_access_comparison.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {

struct SoaAccessComparison {
    std::vector<std::string> column_names{};
    std::vector<AccessIntent> accesses{};
    std::vector<SoaColumnAccessComparison> columns{};
    std::uint64_t element_count{};
    std::uint64_t multiplicity{1};
    SoaAllocationStrategy allocation_strategy{SoaAllocationStrategy::separate_columns};
    bool footprint_exact{};
    AccessFootprintSummary first{};
    AccessFootprintSummary second{};
    std::uint64_t first_allocation_count{};
    std::uint64_t second_allocation_count{};
    std::optional<std::uint64_t> first_total_allocation_bytes{};
    std::optional<std::uint64_t> second_total_allocation_bytes{};
    std::optional<std::uint64_t> first_alignment_padding_bytes{};
    std::optional<std::uint64_t> second_alignment_padding_bytes{};
    std::optional<NumericDelta> allocation_count_delta{};
    std::optional<NumericDelta> total_allocation_byte_delta{};
    std::optional<NumericDelta> alignment_padding_byte_delta{};
    CacheCapacityAnalysis first_minimum_cache_footprint_capacity{};
    CacheCapacityAnalysis second_minimum_cache_footprint_capacity{};
    std::optional<std::uint64_t> first_allocated_capacity_payload_bytes{};
    std::optional<std::uint64_t> second_allocated_capacity_payload_bytes{};
    std::optional<std::uint64_t> first_capacity_slack_payload_bytes{};
    std::optional<std::uint64_t> second_capacity_slack_payload_bytes{};
    std::optional<std::uint64_t> first_full_logical_payload_bytes{};
    std::optional<std::uint64_t> second_full_logical_payload_bytes{};
    std::optional<std::uint64_t> first_unselected_payload_bytes{};
    std::optional<std::uint64_t> second_unselected_payload_bytes{};
    std::optional<std::uint64_t> first_non_payload_cache_bytes{};
    std::optional<std::uint64_t> second_non_payload_cache_bytes{};
    std::optional<std::uint64_t> first_non_payload_page_bytes{};
    std::optional<std::uint64_t> second_non_payload_page_bytes{};
    std::optional<NumericDelta> useful_byte_delta{};
    std::optional<NumericDelta> read_useful_byte_delta{};
    std::optional<NumericDelta> write_useful_byte_delta{};
    std::optional<NumericDelta> logical_read_useful_byte_delta{};
    std::optional<NumericDelta> logical_write_useful_byte_delta{};
    std::optional<NumericDelta> cache_line_delta{};
    std::optional<NumericDelta> cache_byte_delta{};
    std::optional<NumericDelta> read_cache_line_delta{};
    std::optional<NumericDelta> read_cache_byte_delta{};
    std::optional<NumericDelta> write_cache_line_delta{};
    std::optional<NumericDelta> write_cache_byte_delta{};
    std::optional<NumericDelta> page_delta{};
    std::optional<NumericDelta> page_byte_delta{};
    std::optional<NumericDelta> read_page_delta{};
    std::optional<NumericDelta> read_page_byte_delta{};
    std::optional<NumericDelta> write_page_delta{};
    std::optional<NumericDelta> write_page_byte_delta{};
    std::optional<NumericDelta> allocated_capacity_payload_delta{};
    std::optional<NumericDelta> capacity_slack_payload_delta{};
    std::optional<NumericDelta> full_logical_payload_delta{};
    std::optional<NumericDelta> unselected_payload_delta{};
    std::optional<NumericDelta> non_payload_cache_byte_delta{};
    std::optional<NumericDelta> non_payload_page_byte_delta{};
    std::vector<Diagnostic> diagnostics{};
};

}
