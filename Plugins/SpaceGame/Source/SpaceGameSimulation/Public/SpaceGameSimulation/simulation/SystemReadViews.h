#pragma once
#include <sandbox/simulation/capital_death_event.h>
#include <sandbox/simulation/laser_hit_details.h>

#include <sandbox/simulation/laser_soa.h>
#include <SpaceGameSimulation/defences/spinners/TestTubeSpinnersSoA.h>
#include <SpaceGameSimulation/defences/turrets/TestStaticTurretsSoA.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/ships/capital/TestCapitalShipsSoA.h>
#include <SpaceGameSimulation/ships/fighters/TestCapitalShipFightersSoA.h>

#include <span>

enum class EEntityFrameChange : uint8 { Spawn, RemoveSwap };

struct FEntityFrameChange {
    EEntityFrameChange kind{};
    int32 index{};
    FTransform transform{FTransform::Identity};
    ETestTeam team{};
    FRegistryEntityHandle handle{};
};

using FCapitalDeathEvent = ml::simulation::CapitalDeathEvent;

struct FCapitalReadView {
    ml::test_capital_ships::EntityData::ConstView entities;
    FTestEntityRegistry const* registry{};
    TConstArrayView<FRegistryEntityHandle> fighter_handles;
    TConstArrayView<FEntityFrameChange> changes;
    std::span<FCapitalDeathEvent const> deaths;
    auto get_num_instances() const -> int32 { return entities.num(); }
    auto get_fighter_handles(int32 index) const -> TConstArrayView<FRegistryEntityHandle> {
        auto const span{entities.capital_fighter_handle_spans[index]};
        return fighter_handles.Slice(span.offset, span.count);
    }
};
struct FFighterReadView {
    ml::test_capital_ship_fighters::EntityData::ConstView entities;
    FTestEntityRegistry const* registry{};
    auto get_num_instances() const -> int32 { return entities.num(); }
};
struct FTurretReadView {
    ml::test_static_turrets::EntityData::ConstView entities;
    FTestEntityRegistry const* registry{};
    TConstArrayView<FEntityFrameChange> changes;
    TConstArrayView<FVector3f> death_locations;
    auto get_num_instances() const -> int32 { return entities.num(); }
};
struct FSpinnerReadView {
    ml::test_tube_spinners::EntityData::ConstView entities;
    auto get_num_instances() const -> int32 { return entities.num(); }
};
struct FLaserReadView {
    ml::simulation::lasers::Entities::ConstView entities;
    ml::simulation::LaserHitDetailsConstView hits;
    std::span<uint64 const> hit_ticks;
    std::span<int32 const> hit_ordinals;
    auto get_num_instances() const -> int32 { return entities.num(); }
};
