#pragma once

#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/tagged_union_distribution_analysis.hpp>
#include <ioj/layout/analyzer/tagged_union_distribution_entry_comparison.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <optional>
#include <vector>

namespace ioj::layout {

struct TaggedUnionDistributionComparison {
    TaggedUnionDistributionAnalysis first{};
    TaggedUnionDistributionAnalysis second{};
    std::vector<TaggedUnionDistributionEntryComparison> entries{};
    std::optional<NumericDelta> total_weight_delta{};
    std::optional<NumericDelta> total_payload_extent_delta{};
    std::optional<NumericDelta> total_payload_slack_delta{};
    std::optional<long double> expected_payload_extent_per_value_delta{};
    std::optional<long double> expected_payload_slack_per_value_delta{};
    std::optional<long double> expected_selected_payload_extent_delta{};
    std::optional<long double> expected_selected_payload_slack_delta{};
    std::vector<Diagnostic> diagnostics{};
};

}
