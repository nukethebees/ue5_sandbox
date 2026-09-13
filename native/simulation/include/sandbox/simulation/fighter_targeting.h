#pragma once

#include "sandbox/simulation/entity_registry_query.h"

namespace ml::simulation::fighters {
[[nodiscard]] auto select_opportunistic_target(Vector3f location,
                                               Vector3f aim_direction,
                                               std::span<FRegistryEntityHandle const> candidates,
                                               EntityRegistryQueryView registry,
                                               float dot_threshold,
                                               float squared_normal_tolerance) noexcept
    -> FRegistryEntityHandle;
}
