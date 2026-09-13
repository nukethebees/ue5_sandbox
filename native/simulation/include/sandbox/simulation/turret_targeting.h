#pragma once

#include "sandbox/simulation/entity_handle.h"
#include "sandbox/simulation/entity_types.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace ml::simulation {
[[nodiscard]] auto select_turret_target(std::span<FRegistryEntityHandle const> candidates,
                                        std::span<std::uint8_t const> has_line_of_sight,
                                        std::span<std::byte const> registry_teams,
                                        Team turret_team,
                                        std::uint32_t integral_bias) noexcept
    -> FRegistryEntityHandle;
} // namespace ml::simulation
