#pragma once

#include <ioj/layout/analyzer/access_intent.hpp>
#include <ioj/layout/analyzer/cache_capacity_analysis.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {

struct RecordAccessAnalysis {
    lispb::schema::TypeId type{};
    std::vector<std::string> member_names{};
    std::vector<AccessIntent> accesses{};
    std::uint64_t element_count{};
    std::uint64_t multiplicity{1};
    std::optional<std::uint64_t> useful_bytes{};
    std::optional<std::uint64_t> read_useful_bytes{};
    std::optional<std::uint64_t> write_useful_bytes{};
    std::optional<std::uint64_t> logical_read_useful_bytes{};
    std::optional<std::uint64_t> logical_write_useful_bytes{};
    std::optional<std::uint64_t> object_footprint_bytes{};
    std::optional<std::uint64_t> cache_line_bytes{};
    std::optional<std::uint64_t> cache_lines_touched{};
    std::optional<std::uint64_t> cache_bytes_touched{};
    std::optional<std::uint64_t> read_cache_lines_touched{};
    std::optional<std::uint64_t> read_cache_bytes_touched{};
    std::optional<std::uint64_t> write_cache_lines_touched{};
    std::optional<std::uint64_t> write_cache_bytes_touched{};
    std::optional<std::uint64_t> non_selected_cache_bytes{};
    CacheCapacityAnalysis cache_footprint_capacity{};
    std::optional<std::uint64_t> page_bytes{};
    std::optional<std::uint64_t> pages_touched{};
    std::optional<std::uint64_t> page_bytes_touched{};
    std::optional<std::uint64_t> read_pages_touched{};
    std::optional<std::uint64_t> read_page_bytes_touched{};
    std::optional<std::uint64_t> write_pages_touched{};
    std::optional<std::uint64_t> write_page_bytes_touched{};
    std::optional<std::uint64_t> non_selected_page_bytes{};
    std::vector<Diagnostic> diagnostics{};
};

}
