#pragma once

#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/record_access_analysis.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <optional>
#include <vector>

namespace ioj::layout {

struct RecordAccessComparison {
    RecordAccessAnalysis first{};
    RecordAccessAnalysis second{};
    std::optional<NumericDelta> useful_byte_delta{};
    std::optional<NumericDelta> read_useful_byte_delta{};
    std::optional<NumericDelta> write_useful_byte_delta{};
    std::optional<NumericDelta> logical_read_useful_byte_delta{};
    std::optional<NumericDelta> logical_write_useful_byte_delta{};
    std::optional<NumericDelta> object_footprint_byte_delta{};
    std::optional<NumericDelta> cache_line_size_delta{};
    std::optional<NumericDelta> cache_line_delta{};
    std::optional<NumericDelta> cache_byte_delta{};
    std::optional<NumericDelta> read_cache_line_delta{};
    std::optional<NumericDelta> read_cache_byte_delta{};
    std::optional<NumericDelta> write_cache_line_delta{};
    std::optional<NumericDelta> write_cache_byte_delta{};
    std::optional<NumericDelta> non_selected_cache_byte_delta{};
    std::optional<NumericDelta> page_size_delta{};
    std::optional<NumericDelta> page_delta{};
    std::optional<NumericDelta> page_byte_delta{};
    std::optional<NumericDelta> read_page_delta{};
    std::optional<NumericDelta> read_page_byte_delta{};
    std::optional<NumericDelta> write_page_delta{};
    std::optional<NumericDelta> write_page_byte_delta{};
    std::optional<NumericDelta> non_selected_page_byte_delta{};
    std::vector<Diagnostic> diagnostics{};
};

}
