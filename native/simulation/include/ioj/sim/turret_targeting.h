#pragma once

#include "ioj/sim/entity_handle.h"
#include "ioj/sim/entity_types.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace ioj::sim {
[[nodiscard]] auto select_turret_target(std::span<RegistryEntityHandle const> candidates,
                                        std::span<std::uint8_t const> has_line_of_sight,
                                        std::span<std::byte const> registry_teams,
                                        Team turret_team,
                                        std::uint32_t integral_bias) noexcept
    -> RegistryEntityHandle;
} // namespace ioj::sim
