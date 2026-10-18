#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace ioj::layout {

struct UnionDistributionEntryAnalysis {
    std::string alternative_name{};
    std::uint64_t weight{};
    std::optional<std::uint64_t> extent_bytes{};
    std::optional<std::uint64_t> slack_bytes{};
    std::optional<std::uint64_t> weighted_extent_bytes{};
    std::optional<std::uint64_t> weighted_slack_bytes{};
};

}
