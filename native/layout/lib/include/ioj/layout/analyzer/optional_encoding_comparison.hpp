#pragma once

#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/optional_encoding_summary.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace ioj::layout {

struct OptionalEncodingComparison {
    OptionalEncodingSummary first{};
    OptionalEncodingSummary second{};
    std::uint64_t element_count{1};
    bool supported_encodings{};
    bool compatible_source{};
    std::optional<NumericDelta> encoded_storage_bit_delta{};
    std::optional<NumericDelta> total_encoded_bit_delta{};
    std::vector<Diagnostic> diagnostics{};
};

}
