#pragma once

#include "sandbox/core/frame_array.h"
#include "sandbox/simulation/entity_handle.h"
#include "sandbox/simulation/entity_types.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace ml::simulation {
[[nodiscard]] auto find_capital_ship_index(std::span<FRegistryEntityHandle const> handles,
                                           FRegistryEntityHandle handle) noexcept
    -> std::optional<std::int32_t>;

[[nodiscard]] auto find_first_capital_ship_on_team(std::span<std::byte const> teams,
                                                   Team team) noexcept
    -> std::optional<std::int32_t>;

[[nodiscard]] auto
    collect_capitals_without_targets(std::span<FRegistryEntityHandle const> target_handles,
                                     ml::FrameArray<std::int32_t>& output_indices) -> std::int32_t;
} // namespace ml::simulation
