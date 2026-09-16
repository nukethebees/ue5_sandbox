#include "ioj/sim/levels/level_spawn_manager.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <format>
#include <optional>
#include <sandbox/core/diagnostics.h>
#include <span>
#include <vector>

#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/spinners/sim.h>
#include <ioj/sim/turrets/sim.h>

namespace ioj::sim {
LevelSpawnManager::LevelSpawnManager(capital_ships::Sim& capital_ships,
                                     turrets::Sim& turrets,
                                     spinners::Sim& spinners) noexcept
    : capital_ships_{capital_ships}
    , turrets_{turrets}
    , spinners_{spinners} {}

void LevelSpawnManager::initialise(std::int32_t const entity_count,
                                   LevelCapitalSpawnEventsConstView const capital_payloads,
                                   LevelTurretSpawnEventsConstView const turret_payloads) {
    capital_payloads_ = capital_payloads;
    turret_payloads_ = turret_payloads;
    entity_ids_.clear();
    entity_ids_.resize(static_cast<std::size_t>(entity_count));
}

void LevelSpawnManager::set_entity_id(std::int32_t const entity_index, EntityUniqueId const id) {
    assert(entity_index >= 0 && static_cast<std::size_t>(entity_index) < entity_ids_.size());
    assert(id.is_valid());
    entity_ids_[entity_index] = id;
}

void LevelSpawnManager::spawn_initial(LevelCapitalSpawnEventsConstView const capital_events,
                                      LevelTurretSpawnEventsConstView const turret_events,
                                      LevelSpinnerSpawnEventsConstView const spinner_events) {
    if (capital_events.num() > 0) {
        spawn_capitals(capital_events);
    }
    if (turret_events.num() > 0) {
        spawn_turrets(turret_events);
    }
    if (spinner_events.num() > 0) {
        spawn_spinners(spinner_events);
    }
    if (capital_events.num() > 0) {
        resolve_capital_targets(capital_events);
    }
}

void LevelSpawnManager::spawn(LevelSpawnGroupsConstView const groups) {
    auto const group_count{groups.num()};
    for (std::int32_t index{}; index < group_count; ++index) {
        auto const offset{groups.offsets[index]};
        auto const count{groups.counts[index]};
        switch (groups.types[index]) {
            case EntityType::CapitalShip: {
                spawn_capitals(capital_payloads_.get_const_view(offset, count));
                break;
            }
            case EntityType::Turret: {
                spawn_turrets(turret_payloads_.get_const_view(offset, count));
                break;
            }
            default: {
                ml::fatal_error(std::format("Unsupported level spawn entity type: {}",
                                            std::to_underlying(groups.types[index])));
            }
        }
    }
    for (std::int32_t index{}; index < group_count; ++index) {
        if (groups.types[index] == EntityType::CapitalShip) {
            resolve_capital_targets(
                capital_payloads_.get_const_view(groups.offsets[index], groups.counts[index]));
        }
    }
}

void LevelSpawnManager::spawn_capitals(LevelCapitalSpawnEventsConstView const events) {
    auto const count{events.num()};
    target_ids_scratch_.resize(static_cast<std::size_t>(count));
    for (std::int32_t i{}; i < count; ++i) {
        target_ids_scratch_[i] = {};
    }

    auto const size{static_cast<std::size_t>(count)};
    CapitalSpawnDataConstView const spawn_data{
        .target_ids = {target_ids_scratch_.data(), size},
        .locations = events.locations,
        .rotations = events.rotations,
        .teams = events.teams,
        .healths = {events.healths.data(), size},
        .initial_spawn_delays = {events.initial_fighter_spawn_delays.data(), size},
        .spawn_cooldowns = {events.fighter_spawn_cooldowns.data(), size},
    };
    auto const ids{capital_ships_.register_ships(spawn_data)};
    spawned_ids_this_tick_.insert(spawned_ids_this_tick_.end(), ids.begin(), ids.end());
    for (std::int32_t i{}; i < count; ++i) {
        set_entity_id(events.entity_indices[i], ids[i]);
    }
}

void LevelSpawnManager::resolve_capital_targets(LevelCapitalSpawnEventsConstView const events) {
    auto const count{events.num()};
    for (std::int32_t i{}; i < count; ++i) {
        auto const target_index{events.target_entity_indices[i]};
        if (target_index != -1) {
            auto const source_id{get_id(events.entity_indices[i])};
            capital_ships_.set_target_id(source_id, get_id(target_index));
        }
    }
}

void LevelSpawnManager::spawn_turrets(LevelTurretSpawnEventsConstView const events) {
    auto const size{static_cast<std::size_t>(events.num())};
    TurretSpawnDataConstView const spawn_data{
        .locations = events.locations,
        .teams = events.teams,
        .healths = {events.healths.data(), size},
        .laser_damages = {events.laser_damages.data(), size},
    };
    auto const ids{turrets_.register_turrets(spawn_data, events.rotations)};
    spawned_ids_this_tick_.insert(spawned_ids_this_tick_.end(), ids.begin(), ids.end());
    auto const count{events.num()};
    for (std::int32_t i{}; i < count; ++i) {
        set_entity_id(events.entity_indices[i], ids[i]);
    }
}

void LevelSpawnManager::spawn_spinners(LevelSpinnerSpawnEventsConstView const events) {
    auto const ids{spinners_.spawn_instances(
        events.locations, events.yaws, events.initial_fire_point_indices)};
    auto const count{events.num()};
    for (std::int32_t i{}; i < count; ++i) {
        set_entity_id(events.entity_indices[i], ids[i]);
    }
}

auto LevelSpawnManager::get_id(std::int32_t const entity_index) const -> EntityUniqueId {
    assert(entity_index >= 0 && static_cast<std::size_t>(entity_index) < entity_ids_.size());
    auto const id{entity_ids_[entity_index]};
    assert(id.is_valid());
    return id;
}
}
