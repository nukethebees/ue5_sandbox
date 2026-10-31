#pragma once

#include <cstdint>

namespace ioj::sim::telemetry {
struct RunStatistics {
    double simulated_seconds{};
    std::int32_t active_entities{};
    std::int32_t active_lasers{};
    std::int32_t spawned_entities{};
    std::int32_t destroyed_entities{};
    std::int32_t kills{};
    std::int32_t lasers_fired{};
    bool mission_complete{};
    std::uint64_t shots{};
    std::uint64_t hits{};
    double damage_dealt{};
    std::uint64_t combat_kills{};
    std::uint64_t losses{};
};
}
