#pragma once

#include <SpaceGameSimulation/ships/fighters/TestCapitalShipFighterSpawnQueueSoA.h>

#include <SandboxCore/frame_array.h>
#include <SandboxCore/frame_rotators.h>
#include <SandboxCore/frame_vectors.h>

#include <memory_resource>

namespace ml::test_capital_ship_fighters {
struct FrameSpawnQueue {
    explicit FrameSpawnQueue(std::pmr::memory_resource* const resource)
        : locations{resource}
        , rotations{resource}
        , teams{resource}
        , parents{resource}
        , targets{resource} {}

    FrameSpawnQueue(FrameSpawnQueue const&) = delete;
    FrameSpawnQueue(FrameSpawnQueue&&) = delete;
    auto operator=(FrameSpawnQueue const&) -> FrameSpawnQueue& = delete;
    auto operator=(FrameSpawnQueue&&) -> FrameSpawnQueue& = delete;
    ~FrameSpawnQueue() = default;

    void reserve(int32 const count) {
        locations.reserve(count);
        rotations.reserve(count);
        teams.reserve(count);
        parents.reserve(count);
        targets.reserve(count);
    }
    void clear() {
        locations.clear();
        rotations.clear();
        teams.clear();
        parents.clear();
        targets.clear();
    }
    void add(FVector3f const location,
             FRotator3f const rotation,
             ETestTeam const team,
             FRegistryEntityHandle const parent,
             FRegistryEntityHandle const target) {
        locations.add(location);
        rotations.add(rotation);
        teams.add(team);
        parents.add(parent);
        targets.add(target);
    }
    auto get_const_view() const -> TestCapitalShipFighterSpawnQueueConstView {
        return {
            locations.get_const_view(),
            rotations.get_const_view(),
            teams,
            parents,
            targets,
        };
    }

    FFrameVectors3f locations;
    FFrameRotatorsf rotations;
    TFrameArray<ETestTeam> teams;
    TFrameArray<FRegistryEntityHandle> parents;
    TFrameArray<FRegistryEntityHandle> targets;
};
} // namespace ml::test_capital_ship_fighters
