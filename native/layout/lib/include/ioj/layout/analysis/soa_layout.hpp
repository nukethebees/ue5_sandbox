#pragma once
#include <ioj/layout/analysis/row_range.hpp>
#include <ioj/layout/analyzer/soa_allocation_strategy.hpp>

#include <cstdint>
#include <optional>
namespace ioj::layout {
struct SoaLayout {
    auto operator==(SoaLayout const&) const -> bool = default;

    RowRange columns{};
    std::uint64_t capacity{};
    bool capacity_overridden{};
    SoaAllocationStrategy allocation_strategy{SoaAllocationStrategy::separate_columns};
    std::uint64_t allocation_count{};
    std::optional<std::uint64_t> bytes_per_logical_element{};
    std::optional<std::uint64_t> total_payload_bytes{};
    std::optional<std::uint64_t> total_allocation_bytes{};
    std::optional<std::uint64_t> total_alignment_padding_bytes{};
    std::optional<std::uint64_t> allocation_alignment_bytes{};
};
}
