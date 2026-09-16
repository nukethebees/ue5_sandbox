#include "ioj/sim/levels/level_event_manager.h"
#include <cassert>
#include <cstdint>
#include <optional>
#include <sandbox/core/diagnostics.h>
#include <span>
#include <utility>
#include <vector>

#include <ioj/sim/mission_manager.h>

namespace ioj::sim {
LevelEventManager::LevelEventManager(capital_ships::Sim& capital_ships,
                                     turrets::Sim& turrets,
                                     spinners::Sim& spinners,
                                     MissionManager& mission_manager) noexcept
    : spawn_manager_{capital_ships, turrets, spinners}
    , mission_manager_{mission_manager} {}

void LevelEventManager::initialise(CompiledLevelEvents data, EntityUniqueId const player_id) {
    initialisation_ = std::move(data.initialisation);
    schedule_ = std::move(data.schedule);
    auto const event_tick_count{schedule_.execution_ticks.size()};
    assert(event_tick_count == schedule_.event_group_counts.size());

    next_event_index_ = 0;
    spawn_group_offset_ = 0;
    mission_group_offset_ = 0;
    spawn_manager_.initialise(initialisation_.entity_count,
                              schedule_.capital_spawns.get_const_view().columns(),
                              schedule_.turret_spawns.get_const_view().columns());
    if (initialisation_.player_entity_index != -1) {
        spawn_manager_.set_entity_id(initialisation_.player_entity_index, player_id);
    }
    spawn_manager_.spawn_initial(data.initial_spawns.capital_spawns.get_const_view().columns(),
                                 data.initial_spawns.turret_spawns.get_const_view().columns(),
                                 data.initial_spawns.spinner_spawns.get_const_view().columns());

    mission_manager_.bind_level_event_data(schedule_.mission_events.values,
                                           spawn_manager_.get_entity_ids());

    [[maybe_unused]] std::int32_t spawn_group_count{};
    [[maybe_unused]] std::int32_t mission_group_count{};
    std::int32_t mission_tick_count{};
    for (std::int32_t event_index{}; static_cast<std::size_t>(event_index) < event_tick_count;
         ++event_index) {
        if (event_index != 0) {
            assert(schedule_.execution_ticks[event_index - 1] <
                   schedule_.execution_ticks[event_index]);
        }

        auto const counts{schedule_.event_group_counts[event_index]};
        spawn_group_count += counts.spawn_groups;
        mission_group_count += counts.mission_groups;
        if (counts.mission_groups != 0) {
            ++mission_tick_count;
        }
    }
    assert(spawn_group_count == schedule_.spawn_groups.num());
    assert(mission_group_count == schedule_.mission_events.groups.num());
    mission_manager_.set_pending_objective_events(mission_tick_count);
}

void LevelEventManager::execute_tick(SimTick const tick) {
    spawn_manager_.reset_tick_output();
    auto const event_tick_count{schedule_.execution_ticks.size()};
    if (static_cast<std::size_t>(next_event_index_) == event_tick_count) {
        return;
    }
    auto const execution_tick{schedule_.execution_ticks[next_event_index_]};
    if (execution_tick < tick) {
        ml::fatal_error("Level event dispatch skipped a scheduled tick");
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
    if (counts.mission_groups != 0) {
        mission_manager_.consume_level_events(schedule_.mission_events.groups.get_const_view(
            mission_group_offset_, counts.mission_groups));
        mission_group_offset_ += counts.mission_groups;
    }
    ++next_event_index_;
}

auto LevelEventManager::get_spawned_ids() const -> std::span<EntityUniqueId const> {
    return spawn_manager_.get_spawned_ids();
}

void LevelEventManager::configure_mission() {
    if (initialisation_.mission.has_value()) {
        mission_manager_.initialise_level_mission(initialisation_.mission.value(),
                                                  spawn_manager_.get_entity_ids());
    }
}

auto LevelEventManager::get_entity_id(std::int32_t const entity_index) const -> EntityUniqueId {
    return spawn_manager_.get_id(entity_index);
}

auto LevelEventManager::has_future_spawns() const noexcept -> bool {
    auto const event_count{schedule_.event_group_counts.size()};
    for (std::int32_t index{next_event_index_}; static_cast<std::size_t>(index) < event_count;
         ++index) {
        if (schedule_.event_group_counts[index].spawn_groups != 0) {
            return true;
        }
    }
    return false;
}
}
