#pragma once

#include <ioj/layout/analyzer/access_footprint_summary.hpp>
#include <ioj/layout/analyzer/access_intent.hpp>
#include <ioj/layout/analyzer/cache_capacity_analysis.hpp>
#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/soa_allocation_strategy.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {

struct RecordSoaAccessComparison {
    std::vector<std::string> member_names{};
    std::vector<AccessIntent> accesses{};
    std::uint64_t element_count{};
    std::uint64_t multiplicity{1};
    SoaAllocationStrategy soa_allocation_strategy{SoaAllocationStrategy::separate_columns};
    bool soa_footprint_exact{};
    AccessFootprintSummary record{};
    AccessFootprintSummary soa{};
    CacheCapacityAnalysis record_cache_footprint_capacity{};
    CacheCapacityAnalysis soa_minimum_cache_footprint_capacity{};
    std::optional<std::uint64_t> record_non_useful_cache_bytes{};
    std::optional<std::uint64_t> soa_non_useful_cache_bytes{};
    std::optional<std::uint64_t> record_non_useful_page_bytes{};
    std::optional<std::uint64_t> soa_non_useful_page_bytes{};
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
    std::optional<NumericDelta> non_useful_cache_byte_delta{};
    std::optional<NumericDelta> non_useful_page_byte_delta{};
    std::vector<Diagnostic> diagnostics{};
};

}
