#include "ioj/sim/levels/level_spawn_manager.h"

#include <ioj/diagnostics.h>
#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/levels/level_entity_index.h>
#include <ioj/sim/spinners/sim.h>
#include <ioj/sim/turrets/sim.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <vector>

namespace ioj::sim {
LevelSpawnManager::LevelSpawnManager(capital_ships::Sim& capital_ships,
                                     turrets::Sim& turrets,
                                     spinners::Sim& spinners) noexcept
    : capital_ships_{capital_ships}
    , turrets_{turrets}
    , spinners_{spinners} {}

void LevelSpawnManager::initialise(std::uint32_t const entity_count,
                                   LevelCapitalSpawnEvents::ConstView const capital_payloads,
                                   LevelTurretSpawnEvents::ConstView const turret_payloads,
                                   LevelSpinnerSpawnEvents::ConstView const spinner_payloads) {
    capital_payloads_ = capital_payloads;
    turret_payloads_ = turret_payloads;
    spinner_payloads_ = spinner_payloads;
    entity_ids_.clear();
    entity_ids_.resize(static_cast<std::size_t>(entity_count));
}

void LevelSpawnManager::set_entity_id(LevelEntityIndex const entity_index, EntityId const id) {
    assert(static_cast<std::size_t>(entity_index) < entity_ids_.size());
    assert(id.is_valid());
    entity_ids_[entity_index] = id;
}

void LevelSpawnManager::spawn(LevelSpawnGroups::ConstView const groups) {
    auto const group_count{groups.num()};
    auto const types{groups.types()};
    auto const offsets{groups.offsets()};
    auto const counts{groups.counts()};
    for (std::uint32_t index{}; index < group_count; ++index) {
        auto const offset{offsets[index]};
        auto const count{counts[index]};
        switch (types[index]) {
            case EntityType::CapitalShip: {
                // Spawn groups select distinct payload ranges.
                // NOLINTNEXTLINE(ioj-loop-view-accessor-call)
                spawn_capitals(capital_payloads_.get_const_view(offset, count));
                break;
            }
            case EntityType::Turret: {
                // NOLINTNEXTLINE(ioj-loop-view-accessor-call) -- per-group payload.
                spawn_turrets(turret_payloads_.get_const_view(offset, count));
                break;
            }
            case EntityType::TubeSpinner: {
                // NOLINTNEXTLINE(ioj-loop-view-accessor-call) -- per-group payload.
                spawn_spinners(spinner_payloads_.get_const_view(offset, count));
                break;
            }
            default: {
                fatal_error(std::format("Unsupported level spawn entity type: {}",
                                        std::to_underlying(types[index])));
            }
        }
    }
    for (std::uint32_t index{}; index < group_count; ++index) {
        if (types[index] == EntityType::CapitalShip) {
            resolve_capital_targets(
                // NOLINTNEXTLINE(ioj-loop-view-accessor-call) -- per-group payload.
                capital_payloads_.get_const_view(offsets[index], counts[index]));
        }
    }
}

void LevelSpawnManager::spawn_capitals(LevelCapitalSpawnEvents::ConstView const events) {
    auto const count{events.num()};
    auto const ids{capital_ships_.register_ships(events)};
    spawned_ids_this_tick_.insert(spawned_ids_this_tick_.end(), ids.begin(), ids.end());
    auto const entity_indices{events.entity_indices()};

    for (std::uint32_t i{}; i < count; ++i) {
        set_entity_id(entity_indices[i], ids[i]);
    }
}

void LevelSpawnManager::resolve_capital_targets(LevelCapitalSpawnEvents::ConstView const events) {
    auto const count{events.num()};
    auto const entity_indices{events.entity_indices()};
    auto const target_entity_indices{events.target_entity_indices()};

    for (std::uint32_t i{}; i < count; ++i) {
        auto const target_index{target_entity_indices[i]};
        if (target_index != invalid_level_entity_index) {
            auto const source_id{get_id(entity_indices[i])};
            capital_ships_.set_target_id(source_id, get_id(target_index));
        }
    }
}

void LevelSpawnManager::spawn_turrets(LevelTurretSpawnEvents::ConstView const events) {
    auto const ids{turrets_.register_turrets(events)};
    spawned_ids_this_tick_.insert(spawned_ids_this_tick_.end(), ids.begin(), ids.end());
    auto const count{events.num()};
    auto const entity_indices{events.entity_indices()};

    for (std::uint32_t i{}; i < count; ++i) {
        set_entity_id(entity_indices[i], ids[i]);
    }
}

void LevelSpawnManager::spawn_spinners(LevelSpinnerSpawnEvents::ConstView const events) {
    auto const locations{events.view_locations()};
    auto const ids{
        spinners_.spawn_instances(locations, events.yaws(), events.initial_fire_point_indices())};
    spawned_ids_this_tick_.insert(spawned_ids_this_tick_.end(), ids.begin(), ids.end());
    auto const count{events.num()};
    auto const entity_indices{events.entity_indices()};

    for (std::uint32_t i{}; i < count; ++i) {
        set_entity_id(entity_indices[i], ids[i]);
    }
}

auto LevelSpawnManager::get_id(LevelEntityIndex const entity_index) const -> EntityId {
    assert(static_cast<std::size_t>(entity_index) < entity_ids_.size());
    auto const id{entity_ids_[entity_index]};
    assert(id.is_valid());
    return id;
}
}
