#pragma once

#include "sandbox/core/frame_array.h"
#include "sandbox/simulation/entity_handle.h"
#include "sandbox/simulation/entity_registry_query.h"
#include "sandbox/simulation/fighter_reassignment.h"
#include "sandbox/simulation/index_span.h"
#include "sandbox/simulation/spawned_entity_handles.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace ml::simulation {
[[nodiscard]] auto assign_spawned_capital_fighters(
    std::span<FRegistryEntityHandle const> capital_handles,
    std::span<std::byte const> capital_teams,
    SpawnedEntityHandles const& spawned_handles,
    std::span<FRegistryEntityHandle const> spawn_parents,
    std::span<std::byte const> spawn_teams,
    EntityRegistryQueryView registry,
    ml::test_capital_ships::FighterReassignment& reassignments,
    ml::FrameArray<FRegistryEntityHandle>& fighters_to_self_destruct) -> std::int32_t;

[[nodiscard]] auto
    rebuild_capital_fighter_rosters(std::span<FRegistryEntityHandle const> capital_handles,
                                    std::span<FIndexSpan> fighter_spans,
                                    std::span<FRegistryEntityHandle const> previous_fighters,
                                    ml::test_capital_ships::FighterReassignment& reassignments,
                                    std::span<FRegistryEntityHandle> output_fighters)
        -> std::int32_t;

void plan_capital_fighter_reassignment(
    std::span<FRegistryEntityHandle const> capital_handles,
    std::span<std::byte const> capital_teams,
    std::span<FIndexSpan const> fighter_spans,
    std::span<FRegistryEntityHandle const> fighter_handles,
    std::span<std::int32_t const> dying_capital_indices,
    ml::test_capital_ships::FighterReassignment& reassignments,
    ml::FrameArray<FRegistryEntityHandle>& fighters_to_self_destruct);
} // namespace ml::simulation
