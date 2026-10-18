#pragma once

#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/union_distribution_analysis.hpp>
#include <ioj/layout/analyzer/union_distribution_entry_comparison.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <optional>
#include <vector>

namespace ioj::layout {

struct UnionDistributionComparison {
    UnionDistributionAnalysis first{};
    UnionDistributionAnalysis second{};
    std::vector<UnionDistributionEntryComparison> entries{};
    std::optional<NumericDelta> total_weight_delta{};
    std::optional<NumericDelta> total_extent_delta{};
    std::optional<NumericDelta> total_slack_delta{};
    std::optional<long double> expected_extent_per_value_delta{};
    std::optional<long double> expected_slack_per_value_delta{};
    std::optional<long double> expected_selected_extent_delta{};
    std::optional<long double> expected_selected_slack_delta{};
    std::vector<Diagnostic> diagnostics{};
};

}
