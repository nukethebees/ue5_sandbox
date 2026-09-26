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

void LevelSpawnManager::initialise(
    std::int32_t const entity_count,
    SingleAllocationLevelCapitalSpawnEvents::ConstView const capital_payloads,
    SingleAllocationLevelTurretSpawnEvents::ConstView const turret_payloads) {
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

void LevelSpawnManager::spawn_initial(
    SingleAllocationLevelCapitalSpawnEvents::ConstView const capital_events,
    SingleAllocationLevelTurretSpawnEvents::ConstView const turret_events,
    SingleAllocationLevelSpinnerSpawnEvents::ConstView const spinner_events) {
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

void LevelSpawnManager::spawn_capitals(
    SingleAllocationLevelCapitalSpawnEvents::ConstView const events) {
    auto const count{events.num()};
    auto const ids{capital_ships_.register_ships(events)};
    spawned_ids_this_tick_.insert(spawned_ids_this_tick_.end(), ids.begin(), ids.end());
    auto const entity_indices{events.entity_indices()};

    for (std::int32_t i{}; i < count; ++i) {
        set_entity_id(entity_indices[i], ids[i]);
    }
}

void LevelSpawnManager::resolve_capital_targets(
    SingleAllocationLevelCapitalSpawnEvents::ConstView const events) {
    auto const count{events.num()};
    auto const entity_indices{events.entity_indices()};
    auto const target_entity_indices{events.target_entity_indices()};

    for (std::int32_t i{}; i < count; ++i) {
        auto const target_index{target_entity_indices[i]};
        if (target_index != -1) {
            auto const source_id{get_id(entity_indices[i])};
            capital_ships_.set_target_id(source_id, get_id(target_index));
        }
    }
}

void LevelSpawnManager::spawn_turrets(
    SingleAllocationLevelTurretSpawnEvents::ConstView const events) {
    auto const ids{turrets_.register_turrets(events)};
    spawned_ids_this_tick_.insert(spawned_ids_this_tick_.end(), ids.begin(), ids.end());
    auto const count{events.num()};
    auto const entity_indices{events.entity_indices()};

    for (std::int32_t i{}; i < count; ++i) {
        set_entity_id(entity_indices[i], ids[i]);
    }
}

void LevelSpawnManager::spawn_spinners(
    SingleAllocationLevelSpinnerSpawnEvents::ConstView const events) {
    auto const locations{events.view_locations()};
    auto const ids{
        spinners_.spawn_instances(locations, events.yaws(), events.initial_fire_point_indices())};
    auto const count{events.num()};
    auto const entity_indices{events.entity_indices()};

    for (std::int32_t i{}; i < count; ++i) {
        set_entity_id(entity_indices[i], ids[i]);
    }
}

auto LevelSpawnManager::get_id(std::int32_t const entity_index) const -> EntityUniqueId {
    assert(entity_index >= 0 && static_cast<std::size_t>(entity_index) < entity_ids_.size());
    auto const id{entity_ids_[entity_index]};
    assert(id.is_valid());
    return id;
}
}
