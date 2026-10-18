#pragma once

#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/record_analysis.hpp>
#include <ioj/layout/analyzer/record_member_target_comparison.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <optional>
#include <vector>

namespace ioj::layout {

struct RecordTargetComparison {
    RecordAnalysis first{};
    RecordAnalysis second{};
    std::vector<RecordMemberTargetComparison> members{};
    std::optional<NumericDelta> payload_delta{};
    std::optional<NumericDelta> internal_padding_delta{};
    std::optional<NumericDelta> tail_padding_delta{};
    std::optional<NumericDelta> size_delta{};
    std::optional<NumericDelta> alignment_delta{};
    std::optional<NumericDelta> total_storage_delta{};
    std::optional<NumericDelta> total_payload_delta{};
    std::optional<NumericDelta> total_internal_padding_delta{};
    std::optional<NumericDelta> total_tail_padding_delta{};
    std::optional<NumericDelta> total_padding_delta{};
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
