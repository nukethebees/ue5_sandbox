#pragma once

#include <ioj/sim/entities/team_list.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/levels/event_handle.h>
#include <ioj/sim/levels/event_trigger.h>
#include <ioj/sim/levels/level_capital_spawn_events.h>
#include <ioj/sim/levels/level_spawn_groups.h>
#include <ioj/sim/levels/level_spinner_spawn_events.h>
#include <ioj/sim/levels/level_state.h>
#include <ioj/sim/levels/level_turret_spawn_events.h>
#include <ioj/sim/levels/run_end_reason.h>
#include <ioj/sim/levels/start_timer_event.h>
#include <ioj/sim/levels/terminal_event.h>

#include <memory_resource>
#include <optional>
#include <span>
#include <vector>

namespace ioj::sim {
class ConditionManager;
class LevelSpawnManager;
class Timers;
struct SimClock;
struct SpawnContext;
namespace capital_ships {
struct Sim;
}
namespace turrets {
struct Sim;
}
namespace spinners {
struct Sim;
}

struct WinEvent {
    Team winner;
};

class LevelEventManager {
  public:
    using TriggerIndex = std::uint32_t;

    LevelEventManager() = default;
    LevelEventManager(LevelEventManager&& other, std::pmr::memory_resource& memory);
    LevelEventManager(LevelEventManager&&) = default;
    auto operator=(LevelEventManager&&) -> LevelEventManager& = default;

    /* **************************************** */
    // Definitions
    /* **************************************** */
    void add_spawn_group(EntityType type, std::int32_t offset, std::int32_t count);
    auto add_spawn_event(IndexSpan groups) -> EventHandle;
    auto add_stop_event(RunEndReason reason) -> EventHandle;
    auto add_start_timer_event(StartTimerEvent event) -> EventHandle;
    auto add_win_event(Team team) -> EventHandle;
    void add_trigger(ExprHandle condition, std::span<EventHandle const> events);
    auto collect_participating_teams(std::optional<Team> player_team = {}) const -> TeamList;
    auto spawn_count() const -> std::uint32_t {
        return capital_spawns_.num() + turret_spawns_.num() + spinner_spawns_.num();
    }
    auto spawn_groups() -> LevelSpawnGroups& { return spawn_groups_; }
    auto spawn_groups() const -> LevelSpawnGroups const& { return spawn_groups_; }
    auto capital_spawns() -> LevelCapitalSpawnEvents& { return capital_spawns_; }
    auto capital_spawns() const -> LevelCapitalSpawnEvents const& { return capital_spawns_; }
    auto turret_spawns() -> LevelTurretSpawnEvents& { return turret_spawns_; }
    auto turret_spawns() const -> LevelTurretSpawnEvents const& { return turret_spawns_; }
    auto spinner_spawns() -> LevelSpinnerSpawnEvents& { return spinner_spawns_; }
    auto spinner_spawns() const -> LevelSpinnerSpawnEvents const& { return spinner_spawns_; }
    auto triggers() const -> std::span<EventTrigger const> { return triggers_; }
    auto trigger_events(TriggerIndex index) const -> std::span<EventHandle const> {
        auto const range{triggers_[index].events};
        return std::span{trigger_events_}.subspan(range.offset, range.count);
    }
    auto spawn_event(EventHandle event) const -> IndexSpan { return spawn_events_[event.index()]; }
    auto stop_event(EventHandle event) const -> RunEndReason { return stop_events_[event.index()]; }
    auto start_timer_event(EventHandle event) const -> StartTimerEvent {
        return start_timer_events_[event.index()];
    }
    auto win_event(EventHandle event) const -> WinEvent { return win_events_[event.index()]; }

    /* **************************************** */
    // Scheduling and dispatch
    /* **************************************** */
    void collect_ready(ConditionManager const& conditions);
    auto active_triggers() const -> std::span<TriggerIndex const> { return active_triggers_; }
    void enqueue(EventHandle event);
    void enqueue_stop(RunEndReason reason);
    void dispatch_preparation(LevelSpawnManager& spawns,
                              capital_ships::Sim& capitals,
                              turrets::Sim& turrets,
                              spinners::Sim& spinners,
                              SpawnContext context);
    void dispatch_tick_end(Timers& timers,
                           SimClock const& clock,
                           LevelState& state,
                           std::optional<TerminalEvent>& terminal);

    auto consume_preparation_event() -> std::optional<EventHandle>;
    auto preparation_events() const -> std::span<EventHandle const> { return preparation_events_; }
    auto pending_preparation_events() const -> std::span<EventHandle const> {
        return preparation_events().subspan(preparation_read_cursor_);
    }
    auto consume_tick_end_event() -> std::optional<EventHandle>;
    auto tick_end_events() const -> std::span<EventHandle const> { return tick_end_events_; }
    auto pending_tick_end_events() const -> std::span<EventHandle const> {
        return tick_end_events().subspan(tick_end_read_cursor_);
    }

  private:
    LevelSpawnGroups spawn_groups_;
    LevelCapitalSpawnEvents capital_spawns_;
    LevelTurretSpawnEvents turret_spawns_;
    LevelSpinnerSpawnEvents spinner_spawns_;
    std::vector<IndexSpan> spawn_events_;
    std::vector<RunEndReason> stop_events_;
    std::vector<StartTimerEvent> start_timer_events_;
    std::vector<WinEvent> win_events_;

    std::vector<EventTrigger> triggers_;
    std::vector<EventHandle> trigger_events_;
    std::vector<TriggerIndex> active_triggers_;

    std::pmr::vector<EventHandle> preparation_events_;
    std::size_t preparation_read_cursor_{};
    std::pmr::vector<EventHandle> tick_end_events_;
    std::size_t tick_end_read_cursor_{};
};
}
