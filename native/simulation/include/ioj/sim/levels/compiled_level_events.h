#pragma once

#include <ioj/sim/entities/team_list.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/levels/event_handle.h>
#include <ioj/sim/levels/event_trigger.h>
#include <ioj/sim/levels/level_capital_spawn_events.h>
#include <ioj/sim/levels/level_spawn_groups.h>
#include <ioj/sim/levels/level_spinner_spawn_events.h>
#include <ioj/sim/levels/level_turret_spawn_events.h>
#include <ioj/sim/levels/start_timer_event.h>
#include <ioj/sim/telemetry/run_end_reason.h>

#include <optional>
#include <span>
#include <vector>

namespace ioj::sim {
struct WinEvent {
    Team winner;
};

struct CompiledLevelEvents {
    void add_spawn_group(EntityType type, std::int32_t offset, std::int32_t count);
    auto add_spawn_event(IndexSpan groups) -> EventHandle;
    auto add_stop_event(telemetry::RunEndReason reason) -> EventHandle;
    auto add_start_timer_event(StartTimerEvent event) -> EventHandle;
    auto add_win_event(Team team) -> EventHandle;
    void add_trigger(ExprHandle condition, std::span<EventHandle const> events);
    auto collect_participating_teams(std::optional<Team> player_team = {}) const -> TeamList;
    auto spawn_count() const -> std::uint32_t {
        return capital_spawns.num() + turret_spawns.num() + spinner_spawns.num();
    }

    LevelSpawnGroups spawn_groups{};
    LevelCapitalSpawnEvents capital_spawns{};
    LevelTurretSpawnEvents turret_spawns{};
    LevelSpinnerSpawnEvents spinner_spawns{};
    std::vector<EventTrigger> pending_triggers{};
    std::vector<EventHandle> trigger_events{};
    std::vector<IndexSpan> spawn_events{};
    std::vector<telemetry::RunEndReason> stop_events{};
    std::vector<StartTimerEvent> start_timer_events{};
    std::vector<EventHandle> initial_events{};
    std::vector<WinEvent> win_events;
};

}
