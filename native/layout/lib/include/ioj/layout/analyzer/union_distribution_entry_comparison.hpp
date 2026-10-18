#pragma once

#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/union_distribution_entry_analysis.hpp>

#include <optional>
#include <string>

namespace ioj::layout {

struct UnionDistributionEntryComparison {
    std::string alternative_name{};
    UnionDistributionEntryAnalysis first{};
    UnionDistributionEntryAnalysis second{};
    std::optional<NumericDelta> extent_delta{};
    std::optional<NumericDelta> slack_delta{};
    std::optional<NumericDelta> weighted_extent_delta{};
    std::optional<NumericDelta> weighted_slack_delta{};
};

}
