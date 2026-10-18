#pragma once

#include <ioj/layout/analyzer/exact_code_count.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace ioj::layout::analysis_detail {

struct RelationshipExtentAnalysis {
    std::uint64_t target_extent{};
    std::optional<ExactCodeCount> live_values;
    std::optional<ExactCodeCount> required_codes;
    std::uint32_t minimum_required_bits{1};
    bool width_sufficient{};
    std::optional<std::uint64_t> code_space_capacity_limit;
    std::optional<std::uint64_t> capacity_headroom;
    std::optional<std::uint64_t> semantic_capacity_limit;
    std::optional<std::uint64_t> sentinel_capacity_limit;
    std::optional<std::uint64_t> effective_capacity_limit;
    std::optional<std::uint64_t> effective_capacity_headroom;
    std::vector<Diagnostic> diagnostics;
};

}
