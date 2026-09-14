#pragma once

#include "ioj/sim/collision_grid.h"
#include "ioj/sim/collision_grid_entity_storage.h"
#include "ioj/sim/collision_grid_static_storage.h"
#include "ioj/sim/entity_registry_query.h"
#include "ioj/sim/line_traces.h"
#include "ioj/sim/trace_hits.h"

#include <cstdint>
#include <span>

namespace ioj::sim::collision {
enum class TraceEntityFilter : std::uint8_t {
    None,
    ExcludeFighters,
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
                                        std::span<RegistryEntityHandle const> ignored_entities);

void sweep_grid_aabbs(GridGeometry geometry,
                      CollisionGridEntityStorage const& entity_storage,
                      CollisionGridStaticStorage const& static_storage,
                      EntityRegistryQueryView registry,
                      LineTracesConstView centre_paths,
                      Vector3f moving_half_extent,
                      TraceHitsView hits,
                      std::span<RegistryEntityHandle const> ignored_entities = {},
                      TraceEntityFilter entity_filter = TraceEntityFilter::None);
} // namespace ioj::sim::collision
