#pragma once

#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/record_member_analysis.hpp>

#include <optional>
#include <string>

namespace ioj::layout {

struct RecordMemberTargetComparison {
    std::string name{};
    RecordMemberAnalysis first{};
    RecordMemberAnalysis second{};
    std::optional<NumericDelta> element_size_delta{};
    std::optional<NumericDelta> element_alignment_delta{};
    std::optional<NumericDelta> offset_delta{};
    std::optional<NumericDelta> extent_delta{};
    std::optional<NumericDelta> padding_before_delta{};
};

}
