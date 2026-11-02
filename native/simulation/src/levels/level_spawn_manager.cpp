#include "ioj/sim/levels/level_spawn_manager.h"

#include <ioj/column_math.h>
#include <ioj/diagnostics.h>
#include <ioj/rotator_math.h>
#include <ioj/sim/capital_ships/spawn_interface.h>
#include <ioj/sim/fighters/sim.h>
#include <ioj/sim/levels/level_entity_index.h>
#include <ioj/sim/levels/spawn_context.h>
#include <ioj/sim/spinners/spawn_interface.h>
#include <ioj/sim/turrets/spawn_interface.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <vector>

namespace ioj::sim {
LevelSpawnManager::LevelSpawnManager(capital_ships::Sim& capital_ships,
                                     fighters::Sim& fighters,
                                     turrets::Sim& turrets,
                                     spinners::Sim& spinners,
                                     std::span<Transform3d const> const fighter_spawn_transforms)
    : capital_ships_{capital_ships}
    , fighters_{fighters}
    , turrets_{turrets}
    , spinners_{spinners}
    , fighter_spawn_transforms_{fighter_spawn_transforms.begin(), fighter_spawn_transforms.end()} {}

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

void LevelSpawnManager::spawn(LevelSpawnGroups::ConstView const groups,
                              SpawnContext const context) {
    auto const group_count{groups.num()};
    auto const types{groups.types()};
    auto const offsets{groups.offsets()};
    auto const counts{groups.counts()};
    for (std::uint32_t index{}; index < group_count; ++index) {
        auto const offset{offsets[index]};
        auto const count{counts[index].value()};
        switch (types[index]) {
            case EntityType::CapitalShip: {
                // Spawn groups select distinct payload ranges.
                // NOLINTNEXTLINE(ioj-loop-view-accessor-call)
                spawn_capitals(capital_payloads_.const_view(offset, count), context);
                break;
            }
            case EntityType::Turret: {
                // NOLINTNEXTLINE(ioj-loop-view-accessor-call) -- per-group payload.
                spawn_turrets(turret_payloads_.const_view(offset, count), context);
                break;
            }
            case EntityType::TubeSpinner: {
                // NOLINTNEXTLINE(ioj-loop-view-accessor-call) -- per-group payload.
                spawn_spinners(spinner_payloads_.const_view(offset, count), context);
                break;
            }
            default: {
                fatal_error(std::format("Unsupported level spawn entity type: {}",
                                        std::to_underlying(types[index])));
            }
        }
    }
}

void LevelSpawnManager::spawn_capitals(LevelCapitalSpawnEvents::ConstView const events,
                                       SpawnContext const context) {
    auto const count{events.num()};
    auto const ids{capital_ships::SpawnInterface{capital_ships_}.register_ships(events, context)};
    spawned_ids_this_tick_.insert(spawned_ids_this_tick_.end(), ids.begin(), ids.end());
    auto const entity_indices{events.entity_indices()};
    auto const initial_delays{events.initial_fighter_spawn_delays()};
    auto const cooldowns{events.fighter_spawn_cooldowns()};

    for (std::uint32_t i{}; i < count; ++i) {
        set_entity_id(entity_indices[i], ids[i]);
        fighter_spawn_sources_.push_back({ids[i], initial_delays[i], cooldowns[i]});
    }
}

void LevelSpawnManager::queue_fighter_spawns(float const dt,
                                             FrameMemoryResource& scratch_resource) {
    auto const capitals{capital_ships_.get_entities()};
    auto const ids{capitals.entity_ids()};
    std::erase_if(fighter_spawn_sources_, [ids](FighterSpawnSource const& source) {
        return !std::ranges::contains(ids, source.capital_id);
    });

    auto const locations{capitals.view_locations().resolve()};
    auto const rotations{capitals.view_rotations().resolve()};
    auto const teams{capitals.teams()};
    FighterSpawnQueue wave{scratch_resource};
    assert(std::in_range<std::uint32_t>(fighter_spawn_transforms_.size()));
    wave.reserve(static_cast<std::uint32_t>(fighter_spawn_transforms_.size()));
    for (auto& source : fighter_spawn_sources_) {
        source.remaining_delay -= dt;
        if (source.remaining_delay > 0.f) {
            continue;
        }

        auto const found{std::ranges::find(ids, source.capital_id)};
        assert(found != ids.end());
        auto const index{static_cast<std::uint32_t>(found - ids.begin())};
        auto const location{vector_at(locations, index)};
        auto const rotation{rotation_at(rotations, index)};
        Transform3d const transform{
            to_quaternion(Rotator3d{rotation.pitch, rotation.yaw, rotation.roll}),
            {location.x(), location.y(), location.z()},
            {1.0, 1.0, 1.0}};

        wave.reset();
        for (auto const& relative_transform : fighter_spawn_transforms_) {
            auto const spawn_transform{relative_transform * transform};
            auto const spawn_location{Vector3f{spawn_transform.location}};
            auto const spawn_rotation{Rotator3f{spawn_transform.rotator()}};
            wave.add(spawn_location.x(),
                     spawn_location.y(),
                     spawn_location.z(),
                     spawn_rotation.pitch,
                     spawn_rotation.yaw,
                     spawn_rotation.roll,
                     teams[index]);
        }
        fighters_.queue_spawns(wave.const_view());
        source.remaining_delay = source.cooldown;
    }
}

void LevelSpawnManager::spawn_turrets(LevelTurretSpawnEvents::ConstView const events,
                                      SpawnContext const context) {
    auto const ids{turrets::SpawnInterface{turrets_}.register_turrets(events, context)};
    spawned_ids_this_tick_.insert(spawned_ids_this_tick_.end(), ids.begin(), ids.end());
    auto const count{events.num()};
    auto const entity_indices{events.entity_indices()};

    for (std::uint32_t i{}; i < count; ++i) {
        set_entity_id(entity_indices[i], ids[i]);
    }
}

void LevelSpawnManager::spawn_spinners(LevelSpinnerSpawnEvents::ConstView const events,
                                       SpawnContext const context) {
    auto const locations{events.view_locations().resolve()};
    auto const ids{spinners::SpawnInterface{spinners_}.spawn_instances(
        locations, events.yaws(), events.initial_fire_point_indices(), context)};
    spawned_ids_this_tick_.insert(spawned_ids_this_tick_.end(), ids.begin(), ids.end());
    auto const count{events.num()};
    auto const entity_indices{events.entity_indices()};

    for (std::uint32_t i{}; i < count; ++i) {
        set_entity_id(entity_indices[i], ids[i]);
    }
}

auto LevelSpawnManager::id(LevelEntityIndex const entity_index) const -> EntityId {
    assert(static_cast<std::size_t>(entity_index) < entity_ids_.size());
    auto const id{entity_ids_[entity_index]};
    assert(id.is_valid());
    return id;
}
}
