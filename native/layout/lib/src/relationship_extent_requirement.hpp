#pragma once

#include <ioj/layout/analyzer.hpp>

#include <cstdint>
#include <optional>

namespace ioj::layout::analysis_detail {

struct RelationshipExtentRequirement {
    std::optional<ExactCodeCount> live_values;
    std::optional<ExactCodeCount> required_codes;
    std::uint32_t minimum_code_count_bits{1};
};

}
