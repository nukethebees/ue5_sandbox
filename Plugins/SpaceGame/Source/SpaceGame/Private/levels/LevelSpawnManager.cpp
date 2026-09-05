#include "SpaceGame/levels/LevelSpawnManager.h"

#include <SpaceGame/defences/turrets/TestStaticTurretsSimulation.h>
#include <SpaceGame/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

namespace ml {
void FLevelSpawnManager::initialise(int32 const entity_count,
                                    test_capital_ships::Simulation& capital_ships,
                                    test_static_turrets::Simulation& turrets,
                                    FLevelCapitalSpawnEventsConstView const capital_payloads,
                                    FLevelTurretSpawnEventsConstView const turret_payloads) {
    capital_ships_ = &capital_ships;
    turrets_ = &turrets;
    capital_payloads_ = capital_payloads;
    turret_payloads_ = turret_payloads;
    entity_handles_.Reset();
    entity_handles_.SetNum(entity_count);
}

void FLevelSpawnManager::set_entity_handle(int32 const entity_index,
                                           FRegistryEntityHandle const handle) {
    check(entity_handles_.IsValidIndex(entity_index));
    check(handle.is_valid());
    entity_handles_[entity_index] = handle;
}

void FLevelSpawnManager::spawn(FLevelSpawnGroupsConstView const groups) {
    auto const group_count{groups.num()};
    for (int32 index{}; index < group_count; ++index) {
        auto const offset{groups.offsets[index]};
        auto const count{groups.counts[index]};
        switch (groups.types[index]) {
            case ETestEntityType::CapitalShip: {
                spawn_capitals(capital_payloads_.get_const_view(offset, count));
                break;
            }
            case ETestEntityType::Turret: {
                spawn_turrets(turret_payloads_.get_const_view(offset, count));
                break;
            }
            default: {
                UE_LOG(LogSandbox,
                       Fatal,
                       TEXT("Unsupported level spawn entity type: %s"),
                       LexToString(groups.types[index]));
                break;
            }
        }
    }
}

void FLevelSpawnManager::spawn_capitals(FLevelCapitalSpawnEventsConstView const events) {
    auto const count{events.num()};
    target_handles_scratch_.SetNum(count, EAllowShrinking::No);
    for (int32 i{}; i < count; ++i) {
        target_handles_scratch_[i].reset();
    }

    test_capital_ships::SpawnDataConstView const spawn_data{
        .target_handles = target_handles_scratch_,
        .locations = events.locations,
        .rotations = events.rotations,
        .teams = events.teams,
        .healths = events.healths,
        .initial_spawn_delays = events.initial_fighter_spawn_delays,
        .spawn_cooldowns = events.fighter_spawn_cooldowns,
    };
    auto const handles{capital_ships_->register_ships(spawn_data)};
    for (int32 i{}; i < count; ++i) {
        set_entity_handle(events.entity_indices[i], handles[i]);
    }
    for (int32 i{}; i < count; ++i) {
        auto const target_index{events.target_entity_indices[i]};
        if (target_index != INDEX_NONE) {
            capital_ships_->set_target_handle(handles[i], get_handle(target_index));
        }
    }
}

void FLevelSpawnManager::spawn_turrets(FLevelTurretSpawnEventsConstView const events) {
    test_static_turrets::SpawnDataConstView const spawn_data{
        .locations = events.locations,
        .teams = events.teams,
        .healths = events.healths,
        .laser_damages = events.laser_damages,
    };
    auto const handles{turrets_->register_turrets(spawn_data)};
    auto const count{events.num()};
    turrets_->presentation_spawn_transforms.Reserve(count);
    for (int32 i{}; i < count; ++i) {
        set_entity_handle(events.entity_indices[i], handles[i]);
        turrets_->presentation_spawn_transforms.Emplace(
            FRotator{ml::get_rotator3d(events.rotations, i)},
            FVector{ml::get_vector3f(events.locations, i)});
    }
}

auto FLevelSpawnManager::get_handle(int32 const entity_index) const -> FRegistryEntityHandle {
    check(entity_handles_.IsValidIndex(entity_index));
    auto const handle{entity_handles_[entity_index]};
    check(handle.is_valid());
    return handle;
}
}
