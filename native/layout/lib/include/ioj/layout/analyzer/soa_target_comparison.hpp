#pragma once

#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/soa_analysis.hpp>
#include <ioj/layout/analyzer/soa_column_target_comparison.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <optional>
#include <vector>

namespace ioj::layout {

struct SoaTargetComparison {
    bool compatible{};
    SoaAnalysis first{};
    SoaAnalysis second{};
    std::vector<SoaColumnTargetComparison> columns{};
    std::optional<NumericDelta> allocation_count_delta{};
    std::optional<NumericDelta> bytes_per_logical_element_delta{};
    std::optional<NumericDelta> total_payload_delta{};
    std::optional<NumericDelta> total_allocation_delta{};
    std::optional<NumericDelta> total_alignment_padding_delta{};
    std::optional<NumericDelta> allocation_alignment_delta{};
    std::optional<NumericDelta> cache_line_size_delta{};
    std::optional<NumericDelta> page_size_delta{};
    std::optional<NumericDelta> minimum_page_delta{};
    std::vector<Diagnostic> diagnostics{};
};

}
