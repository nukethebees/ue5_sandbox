#pragma once

#include <ioj/layout/analyzer/access_intent.hpp>
#include <ioj/layout/analyzer/cache_capacity_analysis.hpp>
#include <ioj/layout/analyzer/packed_field_access_analysis.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {

struct PackedAccessAnalysis {
    lispb::schema::TypeId type{};
    std::vector<std::string> field_names{};
    std::vector<AccessIntent> accesses{};
    std::vector<PackedFieldAccessAnalysis> fields{};
    std::uint64_t element_count{};
    std::uint64_t multiplicity{1};
    std::optional<std::uint64_t> useful_bits{};
    std::optional<std::uint64_t> read_useful_bits{};
    std::optional<std::uint64_t> write_useful_bits{};
    std::optional<std::uint64_t> logical_read_useful_bits{};
    std::optional<std::uint64_t> logical_write_useful_bits{};
    std::optional<std::uint64_t> storage_footprint_bytes{};
    std::optional<std::uint64_t> storage_footprint_bits{};
    std::optional<std::uint64_t> non_useful_storage_bits{};
    std::optional<std::uint64_t> unselected_field_bits{};
    std::optional<std::uint64_t> reserved_region_bits{};
    std::optional<std::uint64_t> physically_unused_storage_bits{};
    std::optional<std::uint64_t> cache_line_bytes{};
    std::optional<std::uint64_t> minimum_cache_lines_touched{};
    std::optional<std::uint64_t> minimum_cache_bytes_touched{};
    std::optional<std::uint64_t> read_cache_lines_touched{};
    std::optional<std::uint64_t> read_cache_bytes_touched{};
    std::optional<std::uint64_t> write_cache_lines_touched{};
    std::optional<std::uint64_t> write_cache_bytes_touched{};
    CacheCapacityAnalysis cache_footprint_capacity{};
    std::optional<std::uint64_t> page_bytes{};
    std::optional<std::uint64_t> minimum_pages_touched{};
    std::optional<std::uint64_t> minimum_page_bytes_touched{};
    std::optional<std::uint64_t> read_pages_touched{};
    std::optional<std::uint64_t> read_page_bytes_touched{};
    std::optional<std::uint64_t> write_pages_touched{};
    std::optional<std::uint64_t> write_page_bytes_touched{};
    std::vector<Diagnostic> diagnostics{};
};

}
