#pragma once

#include "ioj/sim/entity_handle.h"
#include "ioj/sim/entity_registry_query.h"
#include "ioj/sim/fighter_reassignment.h"
#include "ioj/sim/index_span.h"
#include "ioj/sim/spawned_entity_handles.h"
#include "sandbox/core/frame_array.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace ioj::sim {
[[nodiscard]] auto
    assign_spawned_fighters(std::span<RegistryEntityHandle const> capital_handles,
                            std::span<std::byte const> capital_teams,
                            SpawnedEntityHandles const& spawned_handles,
                            std::span<RegistryEntityHandle const> spawn_parents,
                            std::span<std::byte const> spawn_teams,
                            EntityRegistryQueryView registry,
                            ioj::sim::capital_ships::FighterReassignment& reassignments,
                            ml::FrameArray<RegistryEntityHandle>& fighters_to_self_destruct)
        -> std::int32_t;

[[nodiscard]] auto
    rebuild_fighter_rosters(std::span<RegistryEntityHandle const> capital_handles,
                            std::span<IndexSpan> fighter_spans,
                            std::span<RegistryEntityHandle const> previous_fighters,
                            ioj::sim::capital_ships::FighterReassignment& reassignments,
                            std::span<RegistryEntityHandle> output_fighters) -> std::int32_t;

void plan_fighter_reassignment(std::span<RegistryEntityHandle const> capital_handles,
                               std::span<std::byte const> capital_teams,
                               std::span<IndexSpan const> fighter_spans,
                               std::span<RegistryEntityHandle const> fighter_handles,
                               std::span<std::int32_t const> dying_capital_indices,
                               ioj::sim::capital_ships::FighterReassignment& reassignments,
                               ml::FrameArray<RegistryEntityHandle>& fighters_to_self_destruct);
} // namespace ioj::sim
