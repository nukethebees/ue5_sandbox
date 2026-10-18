#pragma once

#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/tagged_union_alternative_analysis.hpp>

#include <optional>
#include <string>

namespace ioj::layout {

struct TaggedUnionAlternativeTargetComparison {
    std::string name{};
    TaggedUnionAlternativeAnalysis first{};
    TaggedUnionAlternativeAnalysis second{};
    std::optional<NumericDelta> element_size_delta{};
    std::optional<NumericDelta> element_alignment_delta{};
    std::optional<NumericDelta> extent_delta{};
    std::optional<NumericDelta> payload_slack_delta{};
    std::optional<NumericDelta> total_payload_slack_delta{};
};

}
