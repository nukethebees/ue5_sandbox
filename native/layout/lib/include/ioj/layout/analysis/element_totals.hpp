#pragma once
#include <ioj/layout/analyzer/cache_capacity_analysis.hpp>

#include <cstdint>
#include <optional>

namespace ioj::layout {
struct ElementTotals {
    std::uint64_t element_count{};
    std::optional<std::uint64_t> total_storage_bytes{};
    std::optional<std::uint64_t> total_payload_bytes{};
    std::optional<std::uint64_t> total_internal_padding_bytes{};
    std::optional<std::uint64_t> total_tail_padding_bytes{};
    std::optional<std::uint64_t> total_padding_bytes{};
    std::optional<std::uint64_t> total_encoded_bits{};
    std::optional<std::uint64_t> total_payload_bits{};
    std::optional<std::uint64_t> total_reserved_bits{};
    std::optional<std::uint64_t> total_unused_bits{};
    std::optional<std::uint64_t> minimum_total_bytes{};
    std::optional<std::uint64_t> maximum_total_bytes{};
    std::optional<std::uint64_t> minimum_cache_lines{};
    std::optional<std::uint64_t> complete_elements_per_cache_line{};
    std::optional<std::uint64_t> cache_line_straddling_elements{};
    std::optional<std::uint64_t> minimum_pages{};
    std::optional<std::uint64_t> complete_elements_per_page{};
    std::optional<std::uint64_t> page_straddling_elements{};
    CacheCapacityAnalysis cache_capacity{};
};
}
