#include "sandbox/simulation/levels/LevelSpawnManager.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <format>
#include <optional>
#include <sandbox/core/diagnostics.h>
#include <span>
#include <vector>

#include <sandbox/simulation/defences/turrets/TestStaticTurretsSimulation.h>
#include <sandbox/simulation/ships/capital/TestCapitalShipsSimulation.h>

namespace ml {
FLevelSpawnManager::FLevelSpawnManager(test_capital_ships::Simulation& capital_ships,
                                       test_static_turrets::Simulation& turrets) noexcept
    : capital_ships_{capital_ships}
    , turrets_{turrets} {}

void FLevelSpawnManager::initialise(std::int32_t const entity_count,
                                    FLevelCapitalSpawnEventsConstView const capital_payloads,
                                    FLevelTurretSpawnEventsConstView const turret_payloads) {
    capital_payloads_ = capital_payloads;
    turret_payloads_ = turret_payloads;
    entity_handles_.clear();
    entity_handles_.resize(static_cast<std::size_t>(entity_count));
}

void FLevelSpawnManager::set_entity_handle(std::int32_t const entity_index,
                                           FRegistryEntityHandle const handle) {
    assert(entity_index >= 0 && static_cast<std::size_t>(entity_index) < entity_handles_.size());
    assert(handle.is_valid());
    entity_handles_[entity_index] = handle;
}

void FLevelSpawnManager::spawn_initial(FLevelCapitalSpawnEventsConstView const capital_events,
                                       FLevelTurretSpawnEventsConstView const turret_events) {
    if (capital_events.num() > 0) {
        spawn_capitals(capital_events);
    }
    if (turret_events.num() > 0) {
        spawn_turrets(turret_events);
    }
}

void FLevelSpawnManager::spawn(FLevelSpawnGroupsConstView const groups) {
    auto const group_count{groups.num()};
    for (std::int32_t index{}; index < group_count; ++index) {
        auto const offset{groups.offsets[index]};
        auto const count{groups.counts[index]};
        switch (groups.types[index]) {
            case ml::simulation::EntityType::CapitalShip: {
                spawn_capitals(capital_payloads_.get_const_view(offset, count));
                break;
            }
            case ml::simulation::EntityType::Turret: {
                spawn_turrets(turret_payloads_.get_const_view(offset, count));
                break;
            }
            default: {
                ml::fatal_error(std::format("Unsupported level spawn entity type: {}",
                                            std::to_underlying(groups.types[index])));
            }
        }
    }
}

void FLevelSpawnManager::spawn_capitals(FLevelCapitalSpawnEventsConstView const events) {
    auto const count{events.num()};
    target_handles_scratch_.resize(static_cast<std::size_t>(count));
    for (std::int32_t i{}; i < count; ++i) {
        target_handles_scratch_[i].reset();
    }

    auto const size{static_cast<std::size_t>(count)};
    test_capital_ships::SpawnDataConstView const spawn_data{
        .target_handles = {target_handles_scratch_.data(), size},
        .locations = events.locations,
        .rotations = events.rotations,
        .teams = events.teams,
        .healths = {events.healths.data(), size},
        .initial_spawn_delays = {events.initial_fighter_spawn_delays.data(), size},
        .spawn_cooldowns = {events.fighter_spawn_cooldowns.data(), size},
    };
    auto const handles{capital_ships_.register_ships(spawn_data)};
    for (std::int32_t i{}; i < count; ++i) {
        set_entity_handle(events.entity_indices[i], handles[i]);
    }
    for (std::int32_t i{}; i < count; ++i) {
        auto const target_index{events.target_entity_indices[i]};
        if (target_index != -1) {
            capital_ships_.set_target_handle(handles[i], get_handle(target_index));
        }
    }
}

void FLevelSpawnManager::spawn_turrets(FLevelTurretSpawnEventsConstView const events) {
    auto const size{static_cast<std::size_t>(events.num())};
    test_static_turrets::SpawnDataConstView const spawn_data{
        .locations = events.locations,
        .teams = events.teams,
        .healths = {events.healths.data(), size},
        .laser_damages = {events.laser_damages.data(), size},
    };
    auto const handles{turrets_.register_turrets(spawn_data, events.rotations)};
    auto const count{events.num()};
    for (std::int32_t i{}; i < count; ++i) {
        set_entity_handle(events.entity_indices[i], handles[i]);
    }
}

auto FLevelSpawnManager::get_handle(std::int32_t const entity_index) const
    -> FRegistryEntityHandle {
    assert(entity_index >= 0 && static_cast<std::size_t>(entity_index) < entity_handles_.size());
    auto const handle{entity_handles_[entity_index]};
    assert(handle.is_valid());
    return handle;
}
}
