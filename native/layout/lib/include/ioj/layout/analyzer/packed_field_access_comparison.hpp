#pragma once

#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/packed_field_access_analysis.hpp>

#include <optional>
#include <string>

namespace ioj::layout {

struct PackedFieldAccessComparison {
    std::string name{};
    PackedFieldAccessAnalysis first{};
    PackedFieldAccessAnalysis second{};
    std::optional<NumericDelta> bit_width_delta{};
    std::optional<NumericDelta> useful_bit_delta{};
    std::optional<NumericDelta> read_useful_bit_delta{};
    std::optional<NumericDelta> write_useful_bit_delta{};
    std::optional<NumericDelta> logical_read_useful_bit_delta{};
    std::optional<NumericDelta> logical_write_useful_bit_delta{};
};

}
