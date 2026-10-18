#pragma once

#include <ioj/layout/analyzer/cache_capacity_analysis.hpp>
#include <ioj/layout/analyzer/soa_allocation_strategy.hpp>
#include <ioj/layout/analyzer/soa_column_analysis.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace ioj::layout {

struct SoaAnalysis {
    lispb::schema::TypeId type{};
    std::uint64_t capacity{};
    bool capacity_overridden{};
    SoaAllocationStrategy allocation_strategy{SoaAllocationStrategy::separate_columns};
    std::uint64_t allocation_count{};
    std::vector<SoaColumnAnalysis> columns{};
    std::optional<std::uint64_t> bytes_per_logical_element{};
    std::optional<std::uint64_t> total_payload_bytes{};
    std::optional<std::uint64_t> total_allocation_bytes{};
    std::optional<std::uint64_t> total_alignment_padding_bytes{};
    std::optional<std::uint64_t> allocation_alignment_bytes{};
    std::optional<std::uint64_t> cache_line_bytes{};
    std::optional<std::uint64_t> page_bytes{};
    std::optional<std::uint64_t> minimum_pages{};
    CacheCapacityAnalysis cache_capacity{};
    std::vector<Diagnostic> diagnostics{};
};

}
