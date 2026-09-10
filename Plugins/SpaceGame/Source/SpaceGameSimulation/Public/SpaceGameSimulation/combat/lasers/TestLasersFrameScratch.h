#pragma once

#include <SpaceGameSimulation/combat/lasers/TestLasersSoA.h>
#include <SpaceGameSimulation/entities/DirectDamageEvents.h>
#include <SpaceGameSimulation/simulation/FrameTraceHits.h>

#include <SandboxCore/frame_array.h>
#include <SandboxCore/frame_rotators.h>
#include <SandboxCore/frame_vectors.h>

#include <memory_resource>

namespace ml::test_lasers {
struct FrameCollisionScratch {
    explicit FrameCollisionScratch(std::pmr::memory_resource* const resource)
        : trace_starts{resource}
        , trace_ends{resource}
        , trace_hits{resource} {}

    FrameCollisionScratch(FrameCollisionScratch const&) = delete;
    FrameCollisionScratch(FrameCollisionScratch&&) = delete;
    auto operator=(FrameCollisionScratch const&) -> FrameCollisionScratch& = delete;
    auto operator=(FrameCollisionScratch&&) -> FrameCollisionScratch& = delete;
    ~FrameCollisionScratch() = default;

    void set_num(int32 const count) {
        trace_starts.set_num(count);
        trace_ends.set_num(count);
        trace_hits.set_num(count);
    }

    FFrameVectors3f trace_starts;
    FFrameVectors3f trace_ends;
    FFrameTraceHits trace_hits;
};

struct FrameDirectDamageEvents {
    explicit FrameDirectDamageEvents(std::pmr::memory_resource* const resource)
        : damaged_entities{resource}
        , damage_amounts{resource}
        , instigators{resource} {}

    FrameDirectDamageEvents(FrameDirectDamageEvents const&) = delete;
    FrameDirectDamageEvents(FrameDirectDamageEvents&&) = delete;
    auto operator=(FrameDirectDamageEvents const&) -> FrameDirectDamageEvents& = delete;
    auto operator=(FrameDirectDamageEvents&&) -> FrameDirectDamageEvents& = delete;
    ~FrameDirectDamageEvents() = default;

    void reserve(int32 const count) {
        damaged_entities.reserve(count);
        damage_amounts.reserve(count);
        instigators.reserve(count);
    }
    void add(FRegistryEntityHandle const damaged_entity,
             int32 const damage_amount,
             FRegistryEntityHandle const instigator) {
        damaged_entities.add(damaged_entity);
        damage_amounts.add(damage_amount);
        instigators.add(instigator);
    }
    auto get_const_view() const -> DirectDamageEventsConstView {
        return {damaged_entities, damage_amounts, instigators};
    }

    TFrameArray<FRegistryEntityHandle> damaged_entities;
    TFrameArray<int32> damage_amounts;
    TFrameArray<FRegistryEntityHandle> instigators;
};

struct FrameHitDetails {
    explicit FrameHitDetails(std::pmr::memory_resource* const resource)
        : locations{resource}
        , emission_directions{resource}
        , sources{resource} {}

    FrameHitDetails(FrameHitDetails const&) = delete;
    FrameHitDetails(FrameHitDetails&&) = delete;
    auto operator=(FrameHitDetails const&) -> FrameHitDetails& = delete;
    auto operator=(FrameHitDetails&&) -> FrameHitDetails& = delete;
    ~FrameHitDetails() = default;

    void reserve(int32 const count) {
        locations.reserve(count);
        emission_directions.reserve(count);
        sources.reserve(count);
    }
    void add(FVector3f const location,
             FVector3f const emission_direction,
             FLaserSource const source) {
        locations.add(location);
        emission_directions.add(emission_direction);
        sources.add(source);
    }
    auto get_const_view() const -> HitDetailsConstView {
        return {locations.get_const_view(), emission_directions.get_const_view(), sources};
    }
    auto num() const -> int32 { return locations.num(); }

    FFrameVectors3f locations;
    FFrameVectors3f emission_directions;
    TFrameArray<FLaserSource> sources;
};

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
             FLaserSource const source) {
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
    TFrameArray<FLaserSource> sources;
};
} // namespace ml::test_lasers
