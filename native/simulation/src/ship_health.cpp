#include "sandbox/simulation/ship_health.h"

#include <cstddef>

namespace ml::simulation {
auto clamp_health_to_max(std::int32_t const health, std::int32_t const max_health) noexcept
    -> std::int32_t {
    return max_health > health ? max_health : health;
}

auto apply_direct_damage(FRegistryEntityHandle const ship,
                         std::int32_t health,
                         DirectDamageEventsConstView const damage_events) noexcept
    -> ShipDamageResult {
    auto const original_health{health};
    FRegistryEntityHandle killer{};

    auto const damage_count{damage_events.num()};
    for (std::int32_t event_index{}; event_index < damage_count; ++event_index) {
        auto const event_element{static_cast<std::size_t>(event_index)};
        if (damage_events.damaged_entities[event_element] != ship) {
            continue;
        }

        auto const was_alive{health > 0};
        health -= damage_events.damage_amounts[event_element];
        if (was_alive && health <= 0) {
            killer = damage_events.instigators[event_element];
        }
    }

    return {health, killer, original_health > 0 && health <= 0};
}
} // namespace ml::simulation
