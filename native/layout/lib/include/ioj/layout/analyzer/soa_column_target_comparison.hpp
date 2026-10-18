#pragma once

#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/soa_column_analysis.hpp>

#include <optional>
#include <string>

namespace ioj::layout {

struct SoaColumnTargetComparison {
    std::string name{};
    SoaColumnAnalysis first{};
    SoaColumnAnalysis second{};
    std::optional<NumericDelta> element_size_delta{};
    std::optional<NumericDelta> element_alignment_delta{};
    std::optional<NumericDelta> total_byte_delta{};
    std::optional<NumericDelta> allocation_offset_delta{};
    std::optional<NumericDelta> padding_before_delta{};
    std::optional<NumericDelta> minimum_cache_line_delta{};
    std::optional<NumericDelta> elements_per_cache_line_delta{};
    std::optional<NumericDelta> minimum_page_delta{};
    std::optional<NumericDelta> complete_elements_per_page_delta{};
};

}
