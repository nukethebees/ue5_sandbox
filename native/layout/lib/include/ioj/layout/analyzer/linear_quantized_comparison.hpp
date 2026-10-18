#pragma once

#include <ioj/layout/analyzer/linear_quantized_analysis.hpp>
#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace ioj::layout {

struct LinearQuantizedComparison {
    LinearQuantizedAnalysis first{};
    LinearQuantizedAnalysis second{};
    std::uint64_t element_count{1};
    bool compatible_source{};
    std::optional<NumericDelta> encoded_storage_bit_delta{};
    std::optional<NumericDelta> total_code_count_delta{};
    std::optional<NumericDelta> usable_code_count_delta{};
    std::optional<NumericDelta> reserved_code_count_delta{};
    std::optional<bool> clipping_changed{};
    std::optional<long double> resolution_delta{};
    std::optional<long double> maximum_rounding_error_delta{};
    std::optional<std::uint64_t> first_total_encoded_bits{};
    std::optional<std::uint64_t> second_total_encoded_bits{};
    std::optional<NumericDelta> total_encoded_bit_delta{};
    std::vector<Diagnostic> diagnostics{};
};

}
