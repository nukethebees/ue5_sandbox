#include "sandbox/simulation/fighter_navigation_state.h"

#include <cassert>
#include <cstddef>

namespace ml::simulation::fighters {
void reset_navigation_state(NavigationStateView const state,
                            std::int32_t const index,
                            NavigationRiskTier const initial_tier,
                            std::int16_t const period,
                            std::int8_t const direct_choice) {
    assert(index >= 0 && index < state.separation_steering.num());
    auto const element{static_cast<std::size_t>(index)};
    state.separation_steering.set(index, HMM_V3(0.f, 0.f, 0.f));
    state.risk_tiers[element] = static_cast<std::uint8_t>(initial_tier);
    state.lower_risk_scan_counts[element] = 0;
    state.choices[element] = direct_choice;
    state.clear_scan_counts[element] = 0;
    state.periods[element] = period;
    state.remaining_ticks[element] = 0;
}
}
