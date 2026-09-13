#pragma once

#include "sandbox/simulation/direct_damage_events.h"

#include <cstdint>

namespace ml::simulation {
struct ShipDamageResult {
    std::int32_t health;
    FRegistryEntityHandle killer;
    bool died;
};

[[nodiscard]] auto clamp_health_to_max(std::int32_t health, std::int32_t max_health) noexcept
    -> std::int32_t;

[[nodiscard]] auto apply_direct_damage(FRegistryEntityHandle ship,
                                       std::int32_t health,
                                       DirectDamageEventsConstView damage_events) noexcept
    -> ShipDamageResult;
} // namespace ml::simulation
