#pragma once

#include <ioj/layout/analyzer/cache_capacity_analysis.hpp>

#include <cstdint>
#include <optional>

namespace ioj::layout {

struct EnumBackingAggregateAnalysis {
    std::uint64_t element_count{1};
    std::optional<std::uint64_t> total_storage_bytes{};
    std::optional<std::uint64_t> cache_line_bytes{};
    std::optional<std::uint64_t> minimum_cache_lines{};
    std::optional<std::uint64_t> complete_elements_per_cache_line{};
    std::optional<std::uint64_t> cache_line_straddling_elements{};
    std::optional<std::uint64_t> page_bytes{};
    std::optional<std::uint64_t> minimum_pages{};
    std::optional<std::uint64_t> complete_elements_per_page{};
    std::optional<std::uint64_t> page_straddling_elements{};
    CacheCapacityAnalysis cache_capacity{};
};

}
