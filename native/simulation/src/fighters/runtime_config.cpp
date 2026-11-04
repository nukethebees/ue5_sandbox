#include <ioj/sim/fighters/runtime_config.h>

#include <ioj/sim/sim_clock.h>

#include <algorithm>
#include <array>

namespace ioj::sim::fighters {
RuntimeConfig::RuntimeConfig(FighterSimConfig const& new_settings, SimClock const& clock)
    : settings{new_settings}
    , attack_retry_cooldown_ticks{clock.duration_to_tick_period<std::int16_t>(
          Duration::make(settings.attack_retry_cooldown))}
    , fire_cooldown_ticks{clock.duration_to_tick_period<std::int16_t>(
          Duration::make(settings.laser.fire_cooldown))} {
    EnumArray<float, NavigationRiskTier> const frequencies{
        std::array{settings.avoidance_clear_update_frequency.get(),
                   settings.avoidance_update_frequency.get(),
                   settings.avoidance_active_update_frequency.get(),
                   settings.avoidance_immediate_update_frequency.get()}};
    for (auto const tier : EnumTraits<NavigationRiskTier>::values) {
        navigation_tick_periods[tier] =
            clock.frequency_to_tick_period<std::int16_t>(Frequency::make(frequencies[tier]));
    }
    auto const clear_update_interval{static_cast<float>(
        navigation_tick_periods[NavigationRiskTier::Clear] * clock.tick_period())};
    navigation_lookahead_time =
        std::max(settings.avoidance_lookahead_time.get(), clear_update_interval * 1.25f);
}
}
