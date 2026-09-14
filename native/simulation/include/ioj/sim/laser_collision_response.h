#pragma once

#include "ioj/sim/frame_direct_damage_events.h"
#include "ioj/sim/frame_hit_details.h"
#include "ioj/sim/laser_source.h"
#include "ioj/sim/trace_hits.h"
#include "ioj/sim/vectors3f.h"
#include "sandbox/core/frame_array.h"

#include <cstdint>
#include <span>

namespace ioj::sim::lasers {
void prepare_collision_traces(Vectors3fConstView locations,
                              Vectors3fConstView velocities,
                              float delta_time,
                              Vectors3fView trace_starts,
                              Vectors3fView trace_ends) noexcept;

void process_collision_hits(TraceHitsConstView trace_hits,
                            Vectors3fConstView velocities,
                            std::span<std::int32_t const> damages,
                            std::span<RegistryEntityHandle const> instigators,
                            std::span<LaserSource const> sources,
                            ml::FrameArray<std::int32_t>& removal_indices,
                            FrameDirectDamageEvents& damage_events,
                            FrameHitDetails& hit_details);
} // namespace ioj::sim::lasers
