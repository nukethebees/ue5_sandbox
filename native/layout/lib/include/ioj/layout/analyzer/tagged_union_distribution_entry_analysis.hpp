#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace ioj::layout {

struct TaggedUnionDistributionEntryAnalysis {
    std::string tag{};
    std::string alternative_name{};
    std::uint64_t weight{};
    std::optional<std::uint64_t> payload_extent_bytes{};
    std::optional<std::uint64_t> payload_slack_bytes{};
    std::optional<std::uint64_t> weighted_payload_extent_bytes{};
    std::optional<std::uint64_t> weighted_payload_slack_bytes{};
};

}
