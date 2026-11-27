#pragma once

#include <ioj/levels/logic_payloads.h>
#include <ioj/sim/fighters/fighter_spawn_queue.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/levels/change_points_events.h>
#include <ioj/sim/levels/event_handle.h>
#include <ioj/sim/levels/event_trigger.h>
#include <ioj/sim/levels/level_spawn_tables.h>
#include <ioj/sim/levels/level_state.h>
#include <ioj/sim/levels/run_end_reason.h>
#include <ioj/sim/levels/start_timer_event.h>
#include <ioj/sim/levels/terminal_event.h>
#include <ioj/team_array.h>

#include <memory_resource>
#include <optional>
#include <span>
#include <vector>

namespace ioj::sim {
class ConditionManager;
class LevelSpawnManager;
class Timers;
struct SimClock;
struct LevelSpawnContext;

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
    auto add_change_points_event(levels::ChangePointsEffect effect) -> EventHandle;
    auto change_points_event(EventHandle event) const -> levels::ChangePointsEffect {
        auto const index{event.index()};
        return {.team = change_points_events_.teams()[index],
                .delta = change_points_events_.deltas()[index]};
    }

    void add_trigger(ExprHandle condition, std::span<EventHandle const> events);
    auto spawn_count() const -> std::uint32_t {
        return fighter_spawns_.num() + capital_spawns_.num() + turret_spawns_.num() +
               spinner_spawns_.num();
    }

    auto spawn_groups() -> LevelSpawnGroups& { return spawn_groups_; }
    auto spawn_groups() const -> LevelSpawnGroups const& { return spawn_groups_; }
    auto fighter_spawns() -> FighterSpawnQueue& { return fighter_spawns_; }
    auto fighter_spawns() const -> FighterSpawnQueue const& { return fighter_spawns_; }
    auto capital_spawns() -> LevelCapitalSpawnEvents& { return capital_spawns_; }
    auto capital_spawns() const -> LevelCapitalSpawnEvents const& { return capital_spawns_; }
    auto turret_spawns() -> LevelTurretSpawnEvents& { return turret_spawns_; }
    auto turret_spawns() const -> LevelTurretSpawnEvents const& { return turret_spawns_; }
    auto spinner_spawns() -> LevelSpinnerSpawnEvents& { return spinner_spawns_; }
    auto spinner_spawns() const -> LevelSpinnerSpawnEvents const& { return spinner_spawns_; }
    auto triggers() const -> std::span<EventTrigger const> { return triggers_; }
    auto trigger_events() const -> std::span<EventHandle const> { return trigger_events_; }
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
    void dispatch_preparation(LevelSpawnManager& spawns, LevelSpawnContext const& context);
    void dispatch_tick_end(Timers& timers,
                           SimClock const& clock,
                           LevelState& state,
                           std::optional<TerminalEvent>& terminal,
                           TeamArray<TeamPoints>& points);

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
    FighterSpawnQueue fighter_spawns_;
    LevelCapitalSpawnEvents capital_spawns_;
    LevelTurretSpawnEvents turret_spawns_;
    LevelSpinnerSpawnEvents spinner_spawns_;
    std::vector<IndexSpan> spawn_events_;
    std::vector<RunEndReason> stop_events_;
    std::vector<StartTimerEvent> start_timer_events_;
    std::vector<WinEvent> win_events_;
    ChangePointsEvents change_points_events_;

    std::vector<EventTrigger> triggers_;
    std::vector<EventHandle> trigger_events_;
    std::vector<TriggerIndex> active_triggers_;

    std::pmr::vector<EventHandle> preparation_events_;
    std::size_t preparation_read_cursor_{};
    std::pmr::vector<EventHandle> tick_end_events_;
    std::size_t tick_end_read_cursor_{};
};
}
