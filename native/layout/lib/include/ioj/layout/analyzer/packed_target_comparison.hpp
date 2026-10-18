#pragma once

#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/packed_analysis.hpp>
#include <ioj/layout/analyzer/packed_field_target_comparison.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <optional>
#include <vector>

namespace ioj::layout {

struct PackedTargetComparison {
    PackedAnalysis first{};
    PackedAnalysis second{};
    std::vector<PackedFieldTargetComparison> fields{};
    std::optional<NumericDelta> storage_size_delta{};
    std::optional<NumericDelta> storage_alignment_delta{};
    std::optional<NumericDelta> storage_value_bit_delta{};
    std::optional<NumericDelta> storage_bit_delta{};
    std::optional<NumericDelta> bits_used_delta{};
    std::optional<NumericDelta> payload_bit_delta{};
    std::optional<NumericDelta> reserved_bit_delta{};
    std::optional<NumericDelta> unused_bit_delta{};
    std::optional<NumericDelta> overflow_bit_delta{};
    std::optional<NumericDelta> total_storage_delta{};
    std::optional<NumericDelta> total_payload_bit_delta{};
    std::optional<NumericDelta> total_reserved_bit_delta{};
    std::optional<NumericDelta> total_unused_bit_delta{};
    std::optional<NumericDelta> cache_line_size_delta{};
    std::optional<NumericDelta> minimum_cache_line_delta{};
    std::optional<NumericDelta> complete_elements_per_cache_line_delta{};
    std::optional<NumericDelta> cache_line_straddling_delta{};
    std::optional<NumericDelta> page_size_delta{};
    std::optional<NumericDelta> minimum_page_delta{};
    std::optional<NumericDelta> complete_elements_per_page_delta{};
    std::optional<NumericDelta> page_straddling_delta{};
    std::vector<Diagnostic> diagnostics{};
};

}
