#include "ioj/sim/levels/level_event_manager.h"

#include <ioj/diagnostics.h>

#include <cassert>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace ioj::sim {
LevelEventManager::LevelEventManager(capital_ships::Sim& capital_ships,
                                     turrets::Sim& turrets,
                                     spinners::Sim& spinners,
                                     CompiledLevelEvents data)
    : schedule_{std::move(data.schedule)}
    , spawn_manager_{capital_ships, turrets, spinners}
    , player_entity_index_{data.initialisation.player_entity_index}
    , spawn_group_offset_{schedule_.initial_spawn_group_count} {
    assert(schedule_.execution_ticks.size() == schedule_.event_group_counts.size());

    spawn_manager_.initialise(data.initialisation.entity_count,
                              schedule_.capital_spawns.get_const_view(),
                              schedule_.turret_spawns.get_const_view(),
                              schedule_.spinner_spawns.get_const_view());
}

void LevelEventManager::begin(EntityId const player_id) {
    if (player_entity_index_ != invalid_level_entity_index) {
        spawn_manager_.set_entity_id(player_entity_index_, player_id);
    }
    spawn_manager_.spawn(schedule_.spawn_groups.get_const_view(0, spawn_group_offset_));
}

void LevelEventManager::execute_tick(SimTick const tick) {
    spawn_manager_.reset_tick_output();
    auto const event_tick_count{schedule_.execution_ticks.size()};
    if (static_cast<std::size_t>(next_event_index_) == event_tick_count) {
        return;
    }
    auto const execution_tick{schedule_.execution_ticks[next_event_index_]};
    if (execution_tick < tick) {
        fatal_error("Level event dispatch skipped a scheduled tick");
    }
    if (execution_tick != tick) {
        return;
    }

    auto const counts{schedule_.event_group_counts[next_event_index_]};
    if (counts.spawn_groups != 0) {
        spawn_manager_.spawn(
            schedule_.spawn_groups.get_const_view(spawn_group_offset_, counts.spawn_groups));
        spawn_group_offset_ += counts.spawn_groups;
    }
    ++next_event_index_;
}

auto LevelEventManager::get_spawned_ids() const -> std::span<EntityId const> {
    return spawn_manager_.get_spawned_ids();
}

auto LevelEventManager::get_entity_id(LevelEntityIndex const entity_index) const -> EntityId {
    return spawn_manager_.get_id(entity_index);
}

auto LevelEventManager::has_future_spawns() const noexcept -> bool {
    auto const event_count{schedule_.event_group_counts.size()};
    for (std::uint32_t index{next_event_index_}; static_cast<std::size_t>(index) < event_count;
         ++index) {
        if (schedule_.event_group_counts[index].spawn_groups != 0) {
            return true;
        }
    }
    return false;
}
}
