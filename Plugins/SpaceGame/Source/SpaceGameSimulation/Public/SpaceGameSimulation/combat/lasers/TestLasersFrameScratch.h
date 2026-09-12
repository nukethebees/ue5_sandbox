#pragma once

#include <sandbox/simulation/frame_collision_scratch.h>
#include <sandbox/simulation/frame_direct_damage_events.h>
#include <sandbox/simulation/frame_hit_details.h>
#include <sandbox/simulation/laser_source.h>

#include <SpaceGameSimulation/combat/lasers/TestLasersSoA.h>
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

struct FrameSpawnRequests {
    explicit FrameSpawnRequests(std::pmr::memory_resource* const resource)
        : locations{resource}
        , rotations{resource}
        , base_velocities{resource}
        , damages{resource}
        , speeds{resource}
        , max_distances{resource}
        , instigator_handles{resource}
        , sources{resource} {}

    FrameSpawnRequests(FrameSpawnRequests const&) = delete;
    FrameSpawnRequests(FrameSpawnRequests&&) = delete;
    auto operator=(FrameSpawnRequests const&) -> FrameSpawnRequests& = delete;
    auto operator=(FrameSpawnRequests&&) -> FrameSpawnRequests& = delete;
    ~FrameSpawnRequests() = default;

    void reserve(int32 const count) {
        locations.reserve(count);
        rotations.reserve(count);
        base_velocities.reserve(count);
        damages.reserve(count);
        speeds.reserve(count);
        max_distances.reserve(count);
        instigator_handles.reserve(count);
        sources.reserve(count);
    }
    void set_num(int32 const count) {
        locations.set_num(count);
        rotations.set_num(count);
        base_velocities.set_num(count);
        damages.set_num(count);
        speeds.set_num(count);
        max_distances.set_num(count);
        instigator_handles.set_num(count);
        sources.set_num(count);
    }
    void add(FVector3f const location,
             FRotator3f const rotation,
             FVector3f const base_velocity,
             int32 const damage,
             float const speed,
             float const max_distance,
             FRegistryEntityHandle const instigator_handle,
             simulation::LaserSource const source) {
        locations.add(location);
        rotations.add(rotation);
        base_velocities.add(base_velocity);
        damages.add(damage);
        speeds.add(speed);
        max_distances.add(max_distance);
        instigator_handles.add(instigator_handle);
        sources.add(source);
    }
    void set_damages(int32 const value) {
        for (auto& damage : damages) {
            damage = value;
        }
    }
    void set_speeds(float const value) {
        for (auto& speed : speeds) {
            speed = value;
        }
    }
    void set_max_distances(float const value) {
        for (auto& max_distance : max_distances) {
            max_distance = value;
        }
    }
    auto get_const_view() const -> SpawnRequestsConstView {
        return {
            locations.get_const_view(),
            rotations.get_const_view(),
            base_velocities.get_const_view(),
            damages,
            speeds,
            max_distances,
            instigator_handles,
            sources,
        };
    }
    auto num() const -> int32 { return locations.num(); }

    FFrameVectors3f locations;
    FFrameRotatorsf rotations;
    FFrameVectors3f base_velocities;
    TFrameArray<int32> damages;
    TFrameArray<float> speeds;
    TFrameArray<float> max_distances;
    TFrameArray<FRegistryEntityHandle> instigator_handles;
    TFrameArray<simulation::LaserSource> sources;
};
} // namespace ml::test_lasers
