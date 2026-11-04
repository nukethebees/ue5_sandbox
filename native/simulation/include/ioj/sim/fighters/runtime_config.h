#pragma once

#include <ioj/constrained_numbers.h>
#include <ioj/enum_array.h>
#include <ioj/sim/fighters/fighter_navigation_types.h>
#include <ioj/sim/fighters/fighter_sim_config.h>

#include <cstdint>

namespace ioj::sim {
struct SimClock;
}

namespace ioj::sim::fighters {
struct RuntimeConfig {
    static constexpr float minimum_attack_distance_ratio{0.4f};
    static constexpr float desired_attack_distance_ratio{0.5f};
    static constexpr float maximum_attack_distance_ratio{0.6f};

    RuntimeConfig() = default;
    RuntimeConfig(FighterSimConfig const& new_settings, SimClock const& clock);

    FighterSimConfig settings{};
    NonNegative<std::int16_t> attack_retry_cooldown_ticks{};
    NonNegative<std::int16_t> fire_cooldown_ticks{};
    EnumArray<std::int16_t, NavigationRiskTier> navigation_tick_periods{};
    float navigation_lookahead_time{};
};
}
