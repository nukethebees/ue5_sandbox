#pragma once

#include <ioj/layout/analyzer/access_intent.hpp>
#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/packed_access_analysis.hpp>
#include <ioj/layout/analyzer/packed_field_access_comparison.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {

struct PackedAccessComparison {
    std::vector<std::string> field_names{};
    std::vector<AccessIntent> accesses{};
    std::vector<PackedFieldAccessComparison> fields{};
    std::uint64_t element_count{};
    std::uint64_t multiplicity{1};
    PackedAccessAnalysis first{};
    PackedAccessAnalysis second{};
    std::optional<NumericDelta> useful_bit_delta{};
    std::optional<NumericDelta> read_useful_bit_delta{};
    std::optional<NumericDelta> write_useful_bit_delta{};
    std::optional<NumericDelta> logical_read_useful_bit_delta{};
    std::optional<NumericDelta> logical_write_useful_bit_delta{};
    std::optional<NumericDelta> storage_footprint_byte_delta{};
    std::optional<NumericDelta> storage_footprint_bit_delta{};
    std::optional<NumericDelta> non_useful_storage_bit_delta{};
    std::optional<NumericDelta> unselected_field_bit_delta{};
    std::optional<NumericDelta> reserved_region_bit_delta{};
    std::optional<NumericDelta> physically_unused_storage_bit_delta{};
    std::optional<NumericDelta> cache_line_size_delta{};
    std::optional<NumericDelta> cache_line_delta{};
    std::optional<NumericDelta> cache_byte_delta{};
    std::optional<NumericDelta> read_cache_line_delta{};
    std::optional<NumericDelta> read_cache_byte_delta{};
    std::optional<NumericDelta> write_cache_line_delta{};
    std::optional<NumericDelta> write_cache_byte_delta{};
    std::optional<NumericDelta> page_size_delta{};
    std::optional<NumericDelta> page_delta{};
    std::optional<NumericDelta> page_byte_delta{};
    std::optional<NumericDelta> read_page_delta{};
    std::optional<NumericDelta> read_page_byte_delta{};
    std::optional<NumericDelta> write_page_delta{};
    std::optional<NumericDelta> write_page_byte_delta{};
    std::vector<Diagnostic> diagnostics{};
};

}
