#pragma once

#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/union_alternative_target_comparison.hpp>
#include <ioj/layout/analyzer/union_analysis.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <optional>
#include <vector>

namespace ioj::layout {

struct UnionTargetComparison {
    UnionAnalysis first{};
    UnionAnalysis second{};
    std::vector<UnionAlternativeTargetComparison> alternatives{};
    std::optional<NumericDelta> largest_alternative_delta{};
    std::optional<NumericDelta> tail_padding_delta{};
    std::optional<NumericDelta> size_delta{};
    std::optional<NumericDelta> alignment_delta{};
    std::optional<NumericDelta> total_storage_delta{};
    std::optional<NumericDelta> total_tail_padding_delta{};
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
