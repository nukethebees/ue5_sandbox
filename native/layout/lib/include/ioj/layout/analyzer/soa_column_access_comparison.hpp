#pragma once

#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/soa_column_access_analysis.hpp>

#include <optional>
#include <string>

namespace ioj::layout {

struct SoaColumnAccessComparison {
    std::string name{};
    SoaColumnAccessAnalysis first{};
    SoaColumnAccessAnalysis second{};
    std::optional<NumericDelta> element_byte_delta{};
    std::optional<NumericDelta> useful_byte_delta{};
    std::optional<NumericDelta> read_useful_byte_delta{};
    std::optional<NumericDelta> write_useful_byte_delta{};
    std::optional<NumericDelta> logical_read_useful_byte_delta{};
    std::optional<NumericDelta> logical_write_useful_byte_delta{};
    std::optional<NumericDelta> cache_line_delta{};
    std::optional<NumericDelta> cache_byte_delta{};
    std::optional<NumericDelta> non_payload_cache_byte_delta{};
    std::optional<NumericDelta> aligned_cache_line_straddling_element_delta{};
    std::optional<NumericDelta> page_delta{};
    std::optional<NumericDelta> page_byte_delta{};
    std::optional<NumericDelta> non_payload_page_byte_delta{};
    std::optional<NumericDelta> aligned_page_straddling_element_delta{};
    std::optional<NumericDelta> allocated_capacity_payload_delta{};
    std::optional<NumericDelta> capacity_slack_payload_delta{};
};

}
