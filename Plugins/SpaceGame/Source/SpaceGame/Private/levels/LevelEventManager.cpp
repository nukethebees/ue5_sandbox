#include "SpaceGame/levels/LevelEventManager.h"

#include <SpaceGame/missions/TestMissionManager.h>

namespace ml {
void FLevelEventManager::initialise(FCompiledLevelEvents data,
                                    test_capital_ships::Simulation& capital_ships,
                                    test_static_turrets::Simulation& turrets,
                                    FTestMissionManager& mission_manager,
                                    FRegistryEntityHandle const player_handle) {
    initialisation_ = MoveTemp(data.initialisation);
    schedule_ = MoveTemp(data.schedule);
    auto const event_tick_count{schedule_.execution_ticks.Num()};
    check(event_tick_count == schedule_.event_group_counts.Num());

    next_event_index_ = 0;
    spawn_group_offset_ = 0;
    mission_group_offset_ = 0;
    spawn_manager_.initialise(initialisation_.entity_count,
                              capital_ships,
                              turrets,
                              schedule_.capital_spawns.get_const_view(),
                              schedule_.turret_spawns.get_const_view());
    if (initialisation_.player_entity_index != INDEX_NONE) {
        spawn_manager_.set_entity_handle(initialisation_.player_entity_index, player_handle);
    }

    mission_manager_ = &mission_manager;
    mission_manager_->bind_level_event_data(schedule_.mission_events.values,
                                            spawn_manager_.get_entity_handles());

    int32 spawn_group_count{};
    int32 mission_group_count{};
    int32 mission_tick_count{};
    for (int32 event_index{}; event_index < event_tick_count; ++event_index) {
        if (event_index != 0) {
            check(schedule_.execution_ticks[event_index - 1] <
                  schedule_.execution_ticks[event_index]);
        }

        auto const counts{schedule_.event_group_counts[event_index]};
        spawn_group_count += counts.spawn_groups;
        mission_group_count += counts.mission_groups;
        if (counts.mission_groups != 0) {
            ++mission_tick_count;
        }
    }
    check(spawn_group_count == schedule_.spawn_groups.num());
    check(mission_group_count == schedule_.mission_events.groups.num());
    mission_manager_->set_pending_objective_events(mission_tick_count);
}

auto FLevelEventManager::dispatch_tick(uint64 const tick) -> bool {
    auto const event_tick_count{schedule_.execution_ticks.Num()};
    if (next_event_index_ == event_tick_count) {
        return false;
    }
    auto const execution_tick{schedule_.execution_ticks[next_event_index_]};
    checkf(execution_tick >= tick, TEXT("Level event dispatch skipped a scheduled tick"));
    if (execution_tick != tick) {
        return false;
    }

    auto const counts{schedule_.event_group_counts[next_event_index_]};
    if (counts.spawn_groups != 0) {
        spawn_manager_.spawn(
            schedule_.spawn_groups.get_const_view(spawn_group_offset_, counts.spawn_groups));
        spawn_group_offset_ += counts.spawn_groups;
    }
    if (counts.mission_groups != 0) {
        mission_manager_->consume_level_events(schedule_.mission_events.groups.get_const_view(
            mission_group_offset_, counts.mission_groups));
        mission_group_offset_ += counts.mission_groups;
    }
    ++next_event_index_;
    return counts.spawn_groups != 0;
}

void FLevelEventManager::configure_mission() {
    check(mission_manager_);
    if (initialisation_.mission.IsSet()) {
        mission_manager_->initialise_level_mission(initialisation_.mission.GetValue(),
                                                   spawn_manager_.get_entity_handles());
    }
}

auto FLevelEventManager::get_entity_handle(int32 const entity_index) const
    -> FRegistryEntityHandle {
    return spawn_manager_.get_handle(entity_index);
}
}
