#pragma once

#include "sandbox/simulation/direct_damage_events.h"
#include "sandbox/simulation/entity_registry_query.h"

#include <cstddef>
#include <span>

namespace ml::simulation::fighters {
void retarget_from_damage(std::span<FRegistryEntityHandle const> fighter_handles,
                          std::span<std::byte const> fighter_teams,
                          std::span<FRegistryEntityHandle> target_handles,
                          DirectDamageEventsConstView damage_events,
                          EntityRegistryQueryView registry) noexcept;
} // namespace ml::simulation::fighters
