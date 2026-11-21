#include "ioj/sim/levels/level_spawn_manager.h"

#include <ioj/column_math.h>
#include <ioj/diagnostics.h>
#include <ioj/sim/capital_ships/spawn_interface.h>
#include <ioj/sim/entities/entity_instance_handle.h>
#include <ioj/sim/fighters/sim.h>
#include <ioj/sim/levels/fighter_launch_column.h>
#include <ioj/sim/levels/level_entity_ref.h>
#include <ioj/sim/levels/level_event_manager.h>
#include <ioj/sim/levels/spawn_context.h>
#include <ioj/sim/spinners/spawn_interface.h>
#include <ioj/sim/turrets/spawn_interface.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

namespace ioj::sim {
LevelSpawnManager::LevelSpawnManager(std::size_t fighters_per_carrier_launch,
                                     EntityCount max_live_carrier_fighters,
                                     TeamCount team_count,
                                     float clearance_buffer,
                                     std::pmr::memory_resource& memory)
    : entity_ids_{&memory}
    , fighters_per_carrier_launch_{static_cast<SpawnRequestCount>(fighters_per_carrier_launch)}
    , max_live_carrier_fighters_per_team_{team_count == 0 ? 0
                                                          : max_live_carrier_fighters / team_count}
    , carrier_launches_{&memory}
    , launch_placements_{memory}
    , clearance_buffer_{clearance_buffer}
    , fighter_spawn_sources_{&memory} {
    if (!std::in_range<SpawnRequestCount>(fighters_per_carrier_launch)) {
        fatal_error("Fighters per carrier launch exceeds 65535");
    }
}

void LevelSpawnManager::initialise(std::uint32_t const entity_count) {
    entity_ids_.resize(static_cast<std::size_t>(entity_count));
}

void LevelSpawnManager::set_entity_id(LevelEntityRef const entity_index, EntityId const id) {
    assert(entity_index.index() < entity_ids_.size());
    assert(id.is_valid());
    assert(!entity_ids_[entity_index.index()].is_valid());
    entity_ids_[entity_index.index()] = id;
}

void LevelSpawnManager::process_event(EventHandle const event,
                                      LevelEventManager const& events,
                                      LevelSpawnContext const& level) {
    SpawnContext const context{level.fighter_inputs.ledger, level.fighter_inputs.clock};
    auto const range{events.spawn_event(event)};
    auto const groups{events.spawn_groups().const_view(range.offset, range.count)};
    auto const types{groups.types()};
    auto const offsets{groups.offsets()};
    auto const counts{groups.counts()};
    auto const group_count{groups.num()};

    for (std::uint32_t index{}; index < group_count; ++index) {
        auto const offset{offsets[index]};
        auto const count{counts[index].value()};
        switch (types[index]) {
            case EntityType::Fighter:
                spawn_fighters(events.fighter_spawns().const_view(offset, count), level);
                break;
            case EntityType::CapitalShip:
                spawn_capitals(
                    events.capital_spawns().const_view(offset, count), level.capitals, context);
                break;
            case EntityType::Turret:
                spawn_turrets(
                    events.turret_spawns().const_view(offset, count), level.turrets, context);
                break;
            case EntityType::TubeSpinner:
                spawn_spinners(
                    events.spinner_spawns().const_view(offset, count), level.spinners, context);
                break;
            default:
                fatal_error("Unsupported level spawn entity type");
        }
    }
}

void LevelSpawnManager::spawn_fighters(FighterSpawnQueue::CompactConstView const placements,
                                       LevelSpawnContext const& level) {
    auto const refs{placements.entity_indices()};
    if (entity_ids_[refs.front().index()].is_valid()) {
        return;
    }

    auto const ids{level.fighters.spawn(placements, level.fighter_inputs)};
    auto const count{placements.num()};
    for (std::uint32_t row{}; row < count; ++row) {
        set_entity_id(refs[row], ids[row]);
    }
}

void LevelSpawnManager::spawn_capitals(LevelCapitalSpawnEvents::CompactConstView const events,
                                       capital_ships::Sim& capital_ships,
                                       SpawnContext const context) {
    if (entity_ids_[events.entity_indices()[0].index()].is_valid()) {
        return;
    }
    auto const count{events.num()};
    auto const ids{capital_ships::SpawnInterface{capital_ships}.register_ships(events, context)};
    auto const entity_indices{events.entity_indices()};
    auto const initial_delays{events.initial_fighter_spawn_delays()};
    auto const cooldowns{events.fighter_spawn_cooldowns()};

    for (std::uint32_t i{}; i < count; ++i) {
        set_entity_id(entity_indices[i], ids[i]);
        fighter_spawn_sources_.push_back({ids[i], initial_delays[i], cooldowns[i]});
    }
}

void LevelSpawnManager::queue_fighter_spawns(
    float const dt,
    CapitalEntityData::CompactConstView const capitals,
    std::span<EntityInstanceHandle const> const capital_handles,
    collision::EntityAABBs const& bounds) {
    std::erase_if(fighter_spawn_sources_, [capital_handles](FighterSpawnSource const& source) {
        return !capital_handles[source.capital_id.index()].is_valid();
    });

    auto const locations{capitals.view_locations().resolve()};
    auto const rotations{capitals.view_rotations().resolve()};

    for (auto& source : fighter_spawn_sources_) {
        source.remaining_delay -= dt;
        if (source.remaining_delay > 0.f) {
            continue;
        }

        source.remaining_delay = source.cooldown;
        auto const handle{capital_handles[source.capital_id.index()]};
        auto const team{handle.team()};
        if (fighters_per_carrier_launch_ == 0) {
            continue;
        }

        auto const index{handle.index()};
        auto const location{vector_at(locations, index)};
        auto const rotation{rotation_at(rotations, index)};
        carrier_launches_.push_back(
            {team, fighter_launch_column(bounds, location, rotation, clearance_buffer_)});
    }
}

void LevelSpawnManager::commit_carrier_launches(LevelSpawnContext const& level) {
    for (auto const& launch : carrier_launches_) {
        auto const remaining{max_live_carrier_fighters_per_team_ -
                             level.fighters.live_carrier_fighters_[launch.team]};
        assert(remaining >= 0);
        auto const count{static_cast<SpawnRequestCount>(
            std::min<EntityCount>(fighters_per_carrier_launch_, remaining))};
        if (count == 0) {
            continue;
        }

        spawn_carrier_fighters(launch, count, level);
    }
    carrier_launches_.clear();
}

void LevelSpawnManager::spawn_carrier_fighters(CarrierLaunch const& launch,
                                               SpawnRequestCount count,
                                               LevelSpawnContext const& level) {
    launch_placements_.add_uninitialised(count);
    auto const placements{launch_placements_.view()};
    auto const direction{forward_direction(launch.column.heading)};
    for (SpawnRequestCount slot{}; slot < count; ++slot) {
        placements.set(
            slot, LevelEntityRef{}, launch.column.position(slot), direction, launch.team);
    }
    level.fighters.spawn(launch_placements_.const_view(), level.fighter_inputs);
    launch_placements_.reset();
}

void LevelSpawnManager::spawn_turrets(LevelTurretSpawnEvents::CompactConstView const events,
                                      turrets::Sim& turrets,
                                      SpawnContext const context) {
    if (entity_ids_[events.entity_indices()[0].index()].is_valid()) {
        return;
    }
    auto const ids{turrets::SpawnInterface{turrets}.register_turrets(events, context)};
    auto const count{events.num()};
    auto const entity_indices{events.entity_indices()};

    for (std::uint32_t i{}; i < count; ++i) {
        set_entity_id(entity_indices[i], ids[i]);
    }
}

void LevelSpawnManager::spawn_spinners(LevelSpinnerSpawnEvents::CompactConstView const events,
                                       spinners::Sim& spinners,
                                       SpawnContext const context) {
    if (entity_ids_[events.entity_indices()[0].index()].is_valid()) {
        return;
    }
    auto const locations{events.view_locations().resolve()};
    auto const ids{spinners::SpawnInterface{spinners}.spawn_instances(
        locations, events.yaws(), events.initial_fire_point_indices(), context)};
    auto const count{events.num()};
    auto const entity_indices{events.entity_indices()};

    for (std::uint32_t i{}; i < count; ++i) {
        set_entity_id(entity_indices[i], ids[i]);
    }
}

}
