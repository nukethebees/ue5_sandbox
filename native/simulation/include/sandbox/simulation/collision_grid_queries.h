#pragma once

#include "sandbox/simulation/collision_grid.h"
#include "sandbox/simulation/collision_grid_entity_storage.h"
#include "sandbox/simulation/collision_grid_static_storage.h"
#include "sandbox/simulation/entity_registry_query.h"
#include "sandbox/simulation/line_traces.h"
#include "sandbox/simulation/trace_hits.h"

#include <cstdint>
#include <span>

namespace ml::simulation::collision {
enum class TraceEntityFilter : std::uint8_t {
    None,
    ExcludeCapitalShipFighters,
};

void trace_grid_lines(GridGeometry geometry,
                      CollisionGridEntityStorage const& entity_storage,
                      CollisionGridStaticStorage const& static_storage,
                      EntityRegistryQueryView registry,
                      LineTracesConstView traces,
                      TraceHitsView hits);
void trace_grid_lines_ignoring_entities(GridGeometry geometry,
                                        CollisionGridEntityStorage const& entity_storage,
                                        CollisionGridStaticStorage const& static_storage,
                                        EntityRegistryQueryView registry,
                                        LineTracesConstView traces,
                                        TraceHitsView hits,
                                        std::span<FRegistryEntityHandle const> ignored_entities);

void sweep_grid_aabbs(GridGeometry geometry,
                      CollisionGridEntityStorage const& entity_storage,
                      CollisionGridStaticStorage const& static_storage,
                      EntityRegistryQueryView registry,
                      LineTracesConstView centre_paths,
                      Vector3f moving_half_extent,
                      TraceHitsView hits,
                      std::span<FRegistryEntityHandle const> ignored_entities = {},
                      TraceEntityFilter entity_filter = TraceEntityFilter::None);
} // namespace ml::simulation::collision
