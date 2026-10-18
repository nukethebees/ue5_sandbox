#pragma once

#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/tagged_union_distribution_entry_analysis.hpp>

#include <optional>
#include <string>

namespace ioj::layout {

struct TaggedUnionDistributionEntryComparison {
    std::string tag{};
    TaggedUnionDistributionEntryAnalysis first{};
    TaggedUnionDistributionEntryAnalysis second{};
    std::optional<NumericDelta> payload_extent_delta{};
    std::optional<NumericDelta> payload_slack_delta{};
    std::optional<NumericDelta> weighted_payload_extent_delta{};
    std::optional<NumericDelta> weighted_payload_slack_delta{};
};

}
