#pragma once

#include "ioj/sim/direct_damage_events.h"
#include "ioj/sim/entity_registry_query.h"

#include <cstddef>
#include <span>

namespace ioj::sim::fighters {
void retarget_from_damage(std::span<RegistryEntityHandle const> fighter_handles,
                          std::span<std::byte const> fighter_teams,
                          std::span<RegistryEntityHandle> target_handles,
                          DirectDamageEventsConstView damage_events,
                          EntityRegistryQueryView registry) noexcept;
} // namespace ioj::sim::fighters
