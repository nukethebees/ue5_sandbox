#include "ioj/sim/turret_firing.h"

#include "sandbox/core/projectile_intercept.h"
#include "sandbox/core/vector_math.h"
#include "sandbox/core/vector_normalization.h"

#include <cassert>

namespace ioj::sim::turrets {
FiringScratch::FiringScratch(std::pmr::memory_resource* const resource)
    : candidate_indices{resource}
    , hit_handles{resource}
    , starts{resource}
    , ends{resource} {}

void prepare_firing(FiringView const turrets,
                    EntityRegistryQueryView const registry,
                    float const disengage_radius_squared,
                    FiringScratch& scratch) {
    auto const count{turrets.locations.num()};
    assert(turrets.fire_point_locations.num() == count);
    assert(turrets.target_locations.num() == count);
    assert(turrets.targets.size() == static_cast<std::size_t>(count));
    assert(turrets.cooldowns.num() == static_cast<std::size_t>(count));
    scratch.candidate_indices.clear();
    scratch.starts.clear();
    scratch.ends.clear();
    scratch.candidate_indices.reserve(count);
    scratch.starts.reserve(count);
    scratch.ends.reserve(count);

    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        auto& target{turrets.targets[element]};
        if (target.is_null()) {
            continue;
        }
        if (!is_valid_alive(registry, target)) {
            target.reset();
            continue;
        }
        if (!turrets.cooldowns.is_ready(element)) {
            continue;
        }

        auto const target_location{turrets.target_locations[index]};
        if (HMM_LenSqrV3(turrets.locations[index] - target_location) >= disengage_radius_squared) {
            target.reset();
            continue;
        }
        scratch.candidate_indices.add(index);
        scratch.starts.add(turrets.fire_point_locations[index]);
        scratch.ends.add(target_location);
        turrets.cooldowns.restart_counter(element);
    }
    scratch.hit_handles.set_num(scratch.candidate_indices.num());
}

void emit_lasers(FiringView const turrets,
                 FiringScratch const& scratch,
                 float const speed,
                 float const maximum_distance,
                 float const normal_tolerance,
                 lasers::FrameSpawnRequests& requests) {
    auto const count{scratch.candidate_indices.num()};
    assert(scratch.hit_handles.num() == count);
    requests.reserve(count);
    for (std::int32_t candidate{}; candidate < count; ++candidate) {
        auto const index{scratch.candidate_indices[candidate]};
        assert(index >= 0 && index < turrets.locations.num());
        auto const element{static_cast<std::size_t>(index)};
        if (scratch.hit_handles[candidate] != turrets.targets[element]) {
            continue;
        }
        auto const location{turrets.fire_point_locations[index]};
        auto const target_location{turrets.target_locations[index]};
        auto const target_velocity{turrets.target_velocities[index]};
        auto const intercept_time{
            ml::solve_intercept_time(location, target_location, target_velocity, speed)};
        auto const direction{ml::native_math::safe_normal(
            target_location + target_velocity * intercept_time - location, normal_tolerance)};
        Rotator3f rotation{};
        ml::native_math::to_rotations(&rotation.pitch,
                                      &rotation.yaw,
                                      &rotation.roll,
                                      &direction.X,
                                      &direction.Y,
                                      &direction.Z,
                                      1);
        requests.add(location,
                     rotation,
                     HMM_V3(0.f, 0.f, 0.f),
                     turrets.laser_damages[element],
                     speed,
                     maximum_distance,
                     turrets.handles[element],
                     {static_cast<Team>(turrets.teams[element]), EntityType::Turret});
    }
}
}
