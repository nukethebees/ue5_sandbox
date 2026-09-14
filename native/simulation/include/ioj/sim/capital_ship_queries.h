#pragma once

#include "ioj/sim/entity_handle.h"
#include "ioj/sim/entity_types.h"
#include "sandbox/core/frame_array.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace ioj::sim {
[[nodiscard]] auto find_capital_ship_index(std::span<RegistryEntityHandle const> handles,
                                           RegistryEntityHandle handle) noexcept
    -> std::optional<std::int32_t>;

[[nodiscard]] auto find_first_capital_ship_on_team(std::span<std::byte const> teams,
                                                   Team team) noexcept
    -> std::optional<std::int32_t>;

[[nodiscard]] auto
    collect_capitals_without_targets(std::span<RegistryEntityHandle const> target_handles,
                                     ml::FrameArray<std::int32_t>& output_indices) -> std::int32_t;
} // namespace ioj::sim
