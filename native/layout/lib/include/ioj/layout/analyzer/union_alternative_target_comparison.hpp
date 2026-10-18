#pragma once

#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/union_alternative_analysis.hpp>

#include <optional>
#include <string>

namespace ioj::layout {

struct UnionAlternativeTargetComparison {
    std::string name{};
    UnionAlternativeAnalysis first{};
    UnionAlternativeAnalysis second{};
    std::optional<NumericDelta> element_size_delta{};
    std::optional<NumericDelta> element_alignment_delta{};
    std::optional<NumericDelta> extent_delta{};
    std::optional<NumericDelta> slack_delta{};
    std::optional<NumericDelta> total_slack_delta{};
};

}
