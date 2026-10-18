#pragma once

#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/packed_field_analysis.hpp>

#include <optional>
#include <string>

namespace ioj::layout {

struct PackedFieldTargetComparison {
    std::string name{};
    PackedFieldAnalysis first{};
    PackedFieldAnalysis second{};
    std::optional<NumericDelta> least_significant_bit_delta{};
    std::optional<NumericDelta> most_significant_bit_delta{};
    std::optional<NumericDelta> unused_code_delta{};
    std::optional<NumericDelta> relationship_target_extent_delta{};
    std::optional<NumericDelta> relationship_minimum_required_bit_delta{};
    std::optional<NumericDelta> relationship_capacity_headroom_delta{};
    std::optional<NumericDelta> relationship_effective_capacity_limit_delta{};
    std::optional<NumericDelta> relationship_effective_capacity_headroom_delta{};
};

}
