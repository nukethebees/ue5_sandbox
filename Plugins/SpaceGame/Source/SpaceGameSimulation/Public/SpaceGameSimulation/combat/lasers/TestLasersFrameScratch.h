#pragma once

#include <sandbox/simulation/frame_collision_scratch.h>
#include <sandbox/simulation/frame_direct_damage_events.h>
#include <sandbox/simulation/frame_hit_details.h>
#include <sandbox/simulation/frame_laser_spawn_requests.h>
#include <sandbox/simulation/laser_source.h>

#include <SpaceGameSimulation/combat/lasers/TestLasersSoA.h>
#include <SpaceGameSimulation/simulation/NativeRotatorTypes.h>
#include <SpaceGameSimulation/simulation/NativeVectorTypes.h>

#include <SandboxCore/frame_array.h>
#include <SandboxCore/frame_rotators.h>
#include <SandboxCore/frame_vectors.h>

#include <memory_resource>

namespace ml::test_lasers {
using FrameCollisionScratch = simulation::lasers::FrameCollisionScratch;
using FrameDirectDamageEvents = simulation::lasers::FrameDirectDamageEvents;
using FrameHitDetails = simulation::lasers::FrameHitDetails;

inline auto make_hit_details_const_view(FrameHitDetails const& details) -> HitDetailsConstView {
    return {
        ml::to_unreal(details.locations.get_const_view()),
        ml::to_unreal(details.emission_directions.get_const_view()),
        {details.sources.data(), details.sources.num()},
    };
}

inline auto make_spawn_requests_const_view(simulation::lasers::FrameSpawnRequests const& requests)
    -> SpawnRequestsConstView {
    return {
        ml::to_unreal(requests.locations.get_const_view()),
        ml::to_unreal(requests.rotations.get_const_view()),
        ml::to_unreal(requests.base_velocities.get_const_view()),
        {requests.damages.data(), requests.damages.num()},
        {requests.speeds.data(), requests.speeds.num()},
        {requests.max_distances.data(), requests.max_distances.num()},
        {requests.instigator_handles.data(), requests.instigator_handles.num()},
        {requests.sources.data(), requests.sources.num()},
    };
}

struct FrameSpawnRequests : simulation::lasers::FrameSpawnRequests {
    using Base = simulation::lasers::FrameSpawnRequests;

    explicit FrameSpawnRequests(std::pmr::memory_resource* const resource)
        : Base{resource} {}

    FrameSpawnRequests(FrameSpawnRequests const&) = delete;
    FrameSpawnRequests(FrameSpawnRequests&&) = delete;
    auto operator=(FrameSpawnRequests const&) -> FrameSpawnRequests& = delete;
    auto operator=(FrameSpawnRequests&&) -> FrameSpawnRequests& = delete;
    ~FrameSpawnRequests() = default;

    void add(FVector3f const location,
             FRotator3f const rotation,
             FVector3f const base_velocity,
             int32 const damage,
             float const speed,
             float const max_distance,
             FRegistryEntityHandle const instigator_handle,
             simulation::LaserSource const source) {
        Base::add(ml::to_native(location),
                  ml::to_native(rotation),
                  ml::to_native(base_velocity),
                  damage,
                  speed,
                  max_distance,
                  instigator_handle,
                  source);
    }
    auto get_const_view() const -> SpawnRequestsConstView {
        return make_spawn_requests_const_view(*this);
    }
};
} // namespace ml::test_lasers
