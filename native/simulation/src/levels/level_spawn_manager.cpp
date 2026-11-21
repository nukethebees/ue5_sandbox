#include "ioj/sim/levels/level_spawn_manager.h"

#include <ioj/column_math.h>
#include <ioj/diagnostics.h>
#include <ioj/sim/capital_ships/spawn_interface.h>
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
#include <optional>
#include <span>
#include <vector>

namespace ioj::sim {
LevelSpawnManager::LevelSpawnManager(std::size_t wave_size,
                                     collision::EntityAABBs const& bounds,
                                     float clearance_buffer,
                                     std::pmr::memory_resource& memory)
    : entity_ids_{&memory}
    , wave_size_{static_cast<std::uint32_t>(wave_size)}
    , bounds_{bounds}
    , clearance_buffer_{clearance_buffer}
    , fighter_spawn_sources_{&memory} {
    assert(std::in_range<std::uint32_t>(wave_size));
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
                                      capital_ships::Sim& capital_ships,
                                      turrets::Sim& turrets,
                                      spinners::Sim& spinners,
                                      SpawnContext const context) {
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
            case EntityType::CapitalShip:
                spawn_capitals(
                    events.capital_spawns().const_view(offset, count), capital_ships, context);
                break;
            case EntityType::Turret:
                spawn_turrets(events.turret_spawns().const_view(offset, count), turrets, context);
                break;
            case EntityType::TubeSpinner:
                spawn_spinners(
                    events.spinner_spawns().const_view(offset, count), spinners, context);
                break;
            default:
                fatal_error("Unsupported level spawn entity type");
        }
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

void LevelSpawnManager::queue_fighter_spawns(float const dt,
                                             CapitalEntityData::CompactConstView const capitals,
                                             fighters::Sim& fighters,
                                             FrameMemoryResource& scratch_resource) {
    auto const ids{capitals.entity_ids()};
    std::erase_if(fighter_spawn_sources_, [ids](FighterSpawnSource const& source) {
        return !std::ranges::contains(ids, source.capital_id);
    });

    auto const locations{capitals.view_locations().resolve()};
    auto const rotations{capitals.view_rotations().resolve()};
    auto const teams{capitals.teams()};
    FighterSpawnQueue wave{scratch_resource};
    wave.reserve(wave_size_);
    for (auto& source : fighter_spawn_sources_) {
        source.remaining_delay -= dt;
        if (source.remaining_delay > 0.f) {
            continue;
        }

        auto const found{std::ranges::find(ids, source.capital_id)};
        auto const index{static_cast<std::uint32_t>(found - ids.begin())};
        auto const location{vector_at(locations, index)};
        auto const rotation{rotation_at(rotations, index)};
        auto const column{fighter_launch_column(bounds_, location, rotation, clearance_buffer_)};

        wave.reset();
        for (std::uint32_t slot{}; slot < wave_size_; ++slot) {
            auto const spawn_location{column.position(slot)};
            wave.add(spawn_location.x(),
                     spawn_location.y(),
                     spawn_location.z(),
                     column.heading.pitch,
                     column.heading.yaw,
                     column.heading.roll,
                     teams[index]);
        }
        fighters.queue_spawns(wave.const_view());
        source.remaining_delay = source.cooldown;
    }
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
