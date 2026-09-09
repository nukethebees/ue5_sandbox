#pragma once
#include <SpaceGameSimulation/combat/lasers/TestLasersSoA.h>
#include <SpaceGameSimulation/defences/spinners/TestTubeSpinnersSoA.h>
#include <SpaceGameSimulation/defences/turrets/TestStaticTurretsSoA.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/ships/capital/TestCapitalShipsSoA.h>
#include <SpaceGameSimulation/ships/fighters/TestCapitalShipFightersSoA.h>

enum class EEntityFrameChange : uint8 { Spawn, RemoveSwap };

struct FEntityFrameChange {
    EEntityFrameChange kind{};
    int32 index{};
    FTransform transform{FTransform::Identity};
    ETestTeam team{};
    FRegistryEntityHandle handle{};
};

struct FCapitalDeathEvent {
    FVector3f location;
    // Events resolved together share a batch within the frame output.
    int32 batch_index{};
};

struct FCapitalReadView {
    ml::test_capital_ships::EntityData::ConstView entities;
    FTestEntityRegistry const* registry{};
    TConstArrayView<FRegistryEntityHandle> fighter_handles;
    TConstArrayView<FEntityFrameChange> changes;
    TConstArrayView<FCapitalDeathEvent> deaths;
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
    ml::test_lasers::Entities::ConstView entities;
    ml::test_lasers::HitDetails::ConstView hits;
    TConstArrayView<uint64> hit_ticks;
    TConstArrayView<int32> hit_ordinals;
    auto get_num_instances() const -> int32 { return entities.num(); }
};
