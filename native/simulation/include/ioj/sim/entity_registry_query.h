#pragma once

#include "ioj/sim/entity_handle.h"
#include "ioj/sim/entity_types.h"
#include "ioj/sim/health.h"
#include "ioj/sim/vectors3f.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace ioj::sim {
struct EntityRegistryQueryView {
    [[nodiscard]] auto num() const noexcept -> std::int32_t { return locations.num(); }

    Vectors3fConstView locations;
    Vectors3fConstView velocities;
    std::span<std::int32_t const> generations;
    std::span<Health const> healths;
    std::span<std::byte const> teams;
    std::span<std::byte const> entity_types;
};

[[nodiscard]] auto analyse_handle(EntityRegistryQueryView registry,
                                  RegistryEntityHandle handle) noexcept -> RegistryHandleState;
[[nodiscard]] auto is_valid_alive(EntityRegistryQueryView registry,
                                  RegistryEntityHandle handle) noexcept -> bool;

[[nodiscard]] auto collect_entities_in_range(EntityRegistryQueryView registry,
                                             Vector3f origin,
                                             float radius,
                                             std::span<RegistryEntityHandle> out_entities) noexcept
    -> std::int32_t;
[[nodiscard]] auto
    collect_non_team_alive_entities(EntityRegistryQueryView registry,
                                    Team excluded_team,
                                    std::span<RegistryEntityHandle> out_entities) noexcept
    -> std::int32_t;

} // namespace ioj::sim
