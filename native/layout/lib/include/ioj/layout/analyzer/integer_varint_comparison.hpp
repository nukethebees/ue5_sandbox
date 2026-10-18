#pragma once

#include <ioj/layout/analyzer/integer_varint_analysis.hpp>
#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <optional>
#include <vector>

namespace ioj::layout {

struct IntegerVarintComparison {
    IntegerVarintAnalysis first{};
    IntegerVarintAnalysis second{};
    bool compatible_source{};
    std::optional<NumericDelta> minimum_encoded_byte_delta{};
    std::optional<NumericDelta> maximum_encoded_byte_delta{};
    std::optional<NumericDelta> minimum_total_byte_delta{};
    std::optional<NumericDelta> maximum_total_byte_delta{};
    std::vector<Diagnostic> diagnostics{};
};

}
