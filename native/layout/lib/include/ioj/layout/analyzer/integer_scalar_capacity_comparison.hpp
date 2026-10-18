#pragma once

#include <ioj/layout/analyzer/integer_scalar_analysis.hpp>
#include <ioj/layout/analyzer/numeric_delta.hpp>

#include <optional>

namespace ioj::layout {

struct IntegerScalarCapacityComparison {
    IntegerScalarAnalysis first{};
    IntegerScalarAnalysis second{};
    std::optional<NumericDelta> relationship_target_extent_delta{};
    std::optional<NumericDelta> relationship_minimum_required_bit_delta{};
    std::optional<NumericDelta> relationship_code_space_capacity_limit_delta{};
    std::optional<NumericDelta> relationship_capacity_headroom_delta{};
    std::optional<NumericDelta> relationship_semantic_capacity_limit_delta{};
    std::optional<NumericDelta> relationship_sentinel_capacity_limit_delta{};
    std::optional<NumericDelta> relationship_effective_capacity_limit_delta{};
    std::optional<NumericDelta> relationship_effective_capacity_headroom_delta{};
    std::optional<bool> relationship_width_fit_changed{};
};

}
