#pragma once

#include "sandbox/core/frame_array.h"
#include "sandbox/simulation/frame_direct_damage_events.h"
#include "sandbox/simulation/frame_hit_details.h"
#include "sandbox/simulation/laser_source.h"
#include "sandbox/simulation/trace_hits.h"
#include "sandbox/simulation/vectors3f.h"

#include <cstdint>
#include <span>

namespace ml::simulation::lasers {
void prepare_collision_traces(Vectors3fConstView locations,
                              Vectors3fConstView velocities,
                              float delta_time,
                              Vectors3fView trace_starts,
                              Vectors3fView trace_ends) noexcept;

void process_collision_hits(TraceHitsConstView trace_hits,
                            Vectors3fConstView velocities,
                            std::span<std::int32_t const> damages,
                            std::span<FRegistryEntityHandle const> instigators,
                            std::span<LaserSource const> sources,
                            ml::FrameArray<std::int32_t>& removal_indices,
                            FrameDirectDamageEvents& damage_events,
                            FrameHitDetails& hit_details);
} // namespace ml::simulation::lasers
