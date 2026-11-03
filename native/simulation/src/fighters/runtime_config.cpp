#include <ioj/sim/fighters/runtime_config.h>

#include <ioj/sim/sim_clock.h>

#include <algorithm>
#include <array>
#include <cstddef>

namespace ioj::sim::fighters {
RuntimeConfig::RuntimeConfig(FighterSimConfig const& new_settings, SimClock const& clock)
    : settings{new_settings}
    , attack_retry_cooldown_ticks{clock.duration_to_tick_period<std::int16_t>(
          Duration::make(settings.attack_retry_cooldown))}
    , fire_cooldown_ticks{clock.duration_to_tick_period<std::int16_t>(
          Duration::make(settings.laser.fire_cooldown))} {
    std::array<float, enum_count<NavigationRiskTier>()> const frequencies{
        settings.avoidance_clear_update_frequency,
        settings.avoidance_update_frequency,
        settings.avoidance_active_update_frequency,
        settings.avoidance_immediate_update_frequency};
    auto const count{frequencies.size()};
    for (std::size_t index{}; index < count; ++index) {
        navigation_tick_periods[static_cast<NavigationRiskTier>(index)] =
            clock.frequency_to_tick_period<std::int16_t>(Frequency::make(frequencies[index]));
    }
    auto const clear_update_interval{static_cast<float>(
        navigation_tick_periods[NavigationRiskTier::Clear] * clock.tick_period())};
    navigation_lookahead_time =
        std::max(settings.avoidance_lookahead_time.get(), clear_update_interval * 1.25f);
}
}
