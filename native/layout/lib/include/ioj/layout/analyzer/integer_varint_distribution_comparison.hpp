#pragma once

#include <ioj/layout/analyzer/integer_varint_distribution_analysis.hpp>
#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <optional>
#include <vector>

namespace ioj::layout {

struct IntegerVarintDistributionComparison {
    IntegerVarintDistributionAnalysis first{};
    IntegerVarintDistributionAnalysis second{};
    bool compatible_source{};
    std::optional<NumericDelta> total_encoded_byte_delta{};
    std::optional<long double> expected_bytes_per_value_delta{};
    std::optional<long double> expected_selected_bytes_delta{};
    std::vector<Diagnostic> diagnostics{};
};

}
