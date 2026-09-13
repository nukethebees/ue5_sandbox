#pragma once

#include "sandbox/simulation/fighter_navigation.h"

namespace ml::simulation::fighters {
struct NavigationStateView {
    Vectors3fView separation_steering;
    std::span<std::uint8_t> risk_tiers;
    std::span<std::uint8_t> lower_risk_scan_counts;
    std::span<std::int8_t> choices;
    std::span<std::uint8_t> clear_scan_counts;
    std::span<std::int16_t> periods;
    std::span<std::int16_t> remaining_ticks;
};

void reset_navigation_state(NavigationStateView state,
                            std::int32_t index,
                            NavigationRiskTier initial_tier,
                            std::int16_t period,
                            std::int8_t direct_choice);
}
