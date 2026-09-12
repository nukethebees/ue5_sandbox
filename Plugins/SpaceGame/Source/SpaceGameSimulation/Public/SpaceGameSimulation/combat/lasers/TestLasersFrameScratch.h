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
        return {
            ml::to_unreal(locations.get_const_view()),
            ml::to_unreal(rotations.get_const_view()),
            ml::to_unreal(base_velocities.get_const_view()),
            {damages.data(), damages.num()},
            {speeds.data(), speeds.num()},
            {max_distances.data(), max_distances.num()},
            {instigator_handles.data(), instigator_handles.num()},
            {sources.data(), sources.num()},
        };
    }
};
} // namespace ml::test_lasers
