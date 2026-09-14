#include "ioj/sim/laser_collision_response.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <functional>

namespace ioj::sim::lasers {
namespace laser_collision_response_detail {
inline constexpr float safe_normal_tolerance{1.e-8f};

auto make_emission_direction(Vector3f const velocity) noexcept -> Vector3f {
    auto const length_squared{HMM_LenSqrV3(velocity)};
    if (length_squared < safe_normal_tolerance) {
        return HMM_V3(0.0f, 0.0f, -1.0f);
    }

    return velocity * (-1.0f / std::sqrt(length_squared));
}
} // namespace laser_collision_response_detail

void prepare_collision_traces(Vectors3fConstView const locations,
                              Vectors3fConstView const velocities,
                              float const delta_time,
                              Vectors3fView const trace_starts,
                              Vectors3fView const trace_ends) noexcept {
    locations.validate_array_sizes();
    velocities.validate_array_sizes();
    trace_starts.validate_array_sizes();
    trace_ends.validate_array_sizes();
    auto const trace_count{locations.num()};
    assert(velocities.num() == trace_count);
    assert(trace_starts.num() == trace_count);
    assert(trace_ends.num() == trace_count);

    for (std::int32_t trace_index{}; trace_index < trace_count; ++trace_index) {
        auto const start{locations[trace_index]};
        trace_starts.set(trace_index, start);
        trace_ends.set(trace_index, start + velocities[trace_index] * delta_time);
    }
}

void process_collision_hits(TraceHitsConstView const trace_hits,
                            Vectors3fConstView const velocities,
                            std::span<std::int32_t const> const damages,
                            std::span<RegistryEntityHandle const> const instigators,
                            std::span<LaserSource const> const sources,
                            ml::FrameArray<std::int32_t>& removal_indices,
                            FrameDirectDamageEvents& damage_events,
                            FrameHitDetails& hit_details) {
    trace_hits.validate_array_sizes();
    velocities.validate_array_sizes();
    auto const entity_count{trace_hits.num()};
    [[maybe_unused]] auto const storage_count{static_cast<std::size_t>(entity_count)};
    assert(velocities.num() == entity_count);
    assert(damages.size() == storage_count);
    assert(instigators.size() == storage_count);
    assert(sources.size() == storage_count);

    auto const detected_hit_count{static_cast<std::int32_t>(
        std::ranges::count_if(trace_hits.hits, [](auto const hit) { return hit != 0; }))};
    removal_indices.reserve(detected_hit_count);
    damage_events.reserve(detected_hit_count);
    hit_details.reserve(detected_hit_count);

    for (std::int32_t entity_index{}; entity_index < entity_count; ++entity_index) {
        auto const entity_element{static_cast<std::size_t>(entity_index)};
        if (trace_hits.hits[entity_element] == 0) {
            continue;
        }

        removal_indices.add(entity_index);

        auto const damaged_entity{trace_hits.entities[entity_element]};
        if (damaged_entity.is_valid()) {
            damage_events.add(damaged_entity, damages[entity_element], instigators[entity_element]);
        }

        hit_details.add(
            trace_hits.locations[entity_index],
            laser_collision_response_detail::make_emission_direction(velocities[entity_index]),
            sources[entity_element]);
    }

    std::ranges::sort(removal_indices.view(), std::greater{});
}
} // namespace ioj::sim::lasers
