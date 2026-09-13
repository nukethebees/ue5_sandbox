#pragma once

#include "sandbox/simulation/entity_registry_query.h"
#include "sandbox/simulation/vectors3f.h"

#include <cstdint>
#include <span>

namespace ml::simulation {
struct EntityRegistryRefreshViews {
    Vectors3fView locations;
    Vectors3fView velocities;
};

// Returns the first invalid input offset, or -1 when every handle was refreshable.
[[nodiscard]] auto refresh_registry_handles(EntityRegistryQueryView registry,
                                            std::span<FRegistryEntityHandle> handles) noexcept
    -> std::int32_t;

// Null handles publish zeroes. Returns the first non-null inactive input offset, or -1.
[[nodiscard]] auto copy_registry_entity_data(EntityRegistryQueryView registry,
                                             std::span<FRegistryEntityHandle const> handles,
                                             EntityRegistryRefreshViews outputs) noexcept
    -> std::int32_t;
} // namespace ml::simulation
