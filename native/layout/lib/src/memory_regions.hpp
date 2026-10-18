#pragma once

#include <ioj/layout/analyzer/cache_capacity_analysis.hpp>
#include <ioj/layout/analyzer/cache_line_tiling.hpp>
#include <ioj/layout/memory_facts.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace ioj::layout::analysis_detail {

auto cache_capacity_analysis(std::optional<std::uint64_t> const working_set_bytes,
                             MemoryFacts const& memory) -> CacheCapacityAnalysis;

auto minimum_regions(std::uint64_t const bytes, std::uint64_t const region_bytes) -> std::uint64_t;

auto straddling_elements(std::uint64_t const element_bytes,
                         std::uint64_t const element_count,
                         std::uint64_t const region_bytes,
                         std::optional<std::uint64_t> const total_bytes)
    -> std::optional<std::uint64_t>;

auto straddling_elements_at_offset(std::uint64_t const element_bytes,
                                   std::uint64_t const element_count,
                                   std::uint64_t const region_bytes,
                                   std::optional<std::uint64_t> const total_bytes,
                                   std::uint64_t const allocation_offset)
    -> std::optional<std::uint64_t>;

auto touched_regions(std::uint64_t const stride_bytes,
                     std::span<std::pair<std::uint64_t, std::uint64_t> const> const members,
                     std::uint64_t const element_count,
                     std::uint64_t const region_bytes,
                     std::string& error) -> std::optional<std::uint64_t>;

auto cache_line_tiling(std::uint64_t const element_bytes, std::uint64_t const cache_line_bytes)
    -> CacheLineTiling;

}
