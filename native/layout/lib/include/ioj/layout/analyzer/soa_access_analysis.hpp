#pragma once

#include <ioj/layout/analyzer/access_intent.hpp>
#include <ioj/layout/analyzer/cache_capacity_analysis.hpp>
#include <ioj/layout/analyzer/soa_allocation_strategy.hpp>
#include <ioj/layout/analyzer/soa_column_access_analysis.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {

struct SoaAccessAnalysis {
    std::vector<std::string> column_names{};
    std::vector<AccessIntent> accesses{};
    std::vector<SoaColumnAccessAnalysis> columns{};
    std::uint64_t element_count{};
    std::uint64_t multiplicity{1};
    SoaAllocationStrategy allocation_strategy{SoaAllocationStrategy::separate_columns};
    bool footprint_exact{};
    std::uint64_t allocation_count{};
    std::optional<std::uint64_t> useful_bytes{};
    std::optional<std::uint64_t> read_useful_bytes{};
    std::optional<std::uint64_t> write_useful_bytes{};
    std::optional<std::uint64_t> logical_read_useful_bytes{};
    std::optional<std::uint64_t> logical_write_useful_bytes{};
    std::optional<std::uint64_t> full_logical_payload_bytes{};
    std::optional<std::uint64_t> unselected_payload_bytes{};
    std::optional<std::uint64_t> allocated_capacity_payload_bytes{};
    std::optional<std::uint64_t> capacity_slack_payload_bytes{};
    std::optional<std::uint64_t> total_allocation_bytes{};
    std::optional<std::uint64_t> alignment_padding_bytes{};
    std::optional<std::uint64_t> cache_line_bytes{};
    std::optional<std::uint64_t> minimum_cache_lines_touched{};
    std::optional<std::uint64_t> minimum_cache_bytes_touched{};
    std::optional<std::uint64_t> minimum_read_cache_lines_touched{};
    std::optional<std::uint64_t> minimum_read_cache_bytes_touched{};
    std::optional<std::uint64_t> minimum_write_cache_lines_touched{};
    std::optional<std::uint64_t> minimum_write_cache_bytes_touched{};
    std::optional<std::uint64_t> non_payload_cache_bytes{};
    CacheCapacityAnalysis minimum_cache_footprint_capacity{};
    std::optional<std::uint64_t> page_bytes{};
    std::optional<std::uint64_t> minimum_pages_touched{};
    std::optional<std::uint64_t> minimum_page_bytes_touched{};
    std::optional<std::uint64_t> minimum_read_pages_touched{};
    std::optional<std::uint64_t> minimum_read_page_bytes_touched{};
    std::optional<std::uint64_t> minimum_write_pages_touched{};
    std::optional<std::uint64_t> minimum_write_page_bytes_touched{};
    std::optional<std::uint64_t> non_payload_page_bytes{};
    std::vector<Diagnostic> diagnostics{};
};

}
