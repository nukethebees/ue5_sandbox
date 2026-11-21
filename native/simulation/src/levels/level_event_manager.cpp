#include <ioj/sim/levels/level_event_manager.h>

#include <ioj/diagnostics.h>
#include <ioj/sim/expressions/condition_manager.h>
#include <ioj/sim/expressions/timers.h>
#include <ioj/sim/levels/level_spawn_manager.h>
#include <ioj/sim/sim_clock.h>

#include <algorithm>
#include <cassert>
#include <limits>
#include <utility>

namespace ioj::sim {
/* **************************************** */
// Construction
/* **************************************** */
// Moving each member lets the event queues use the level's memory resource.
// NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
LevelEventManager::LevelEventManager(LevelEventManager&& other, std::pmr::memory_resource& memory)
    : spawn_groups_{std::move(other.spawn_groups_)}
    , fighter_spawns_{std::move(other.fighter_spawns_)}
    , capital_spawns_{std::move(other.capital_spawns_)}
    , turret_spawns_{std::move(other.turret_spawns_)}
    , spinner_spawns_{std::move(other.spinner_spawns_)}
    , spawn_events_{std::move(other.spawn_events_)}
    , stop_events_{std::move(other.stop_events_)}
    , start_timer_events_{std::move(other.start_timer_events_)}
    , win_events_{std::move(other.win_events_)}
    , triggers_{std::move(other.triggers_)}
    , trigger_events_{std::move(other.trigger_events_)}
    , active_triggers_{std::move(other.active_triggers_)}
    , preparation_events_{std::move(other.preparation_events_), &memory}
    , preparation_read_cursor_{other.preparation_read_cursor_}
    , tick_end_events_{std::move(other.tick_end_events_), &memory}
    , tick_end_read_cursor_{other.tick_end_read_cursor_} {}

/* **************************************** */
// Definitions
/* **************************************** */
void LevelEventManager::add_spawn_group(EntityType const type,
                                        std::int32_t offset,
                                        std::int32_t count) {
    assert(offset >= 0 && count >= 0);

    constexpr auto max_count{std::numeric_limits<LevelEventCount::value_type>::max()};
    while (count > 0) {
        auto const added{std::min(count, static_cast<std::int32_t>(max_count))};
        spawn_groups_.add(
            type, offset, LevelEventCount{static_cast<LevelEventCount::value_type>(added)});
        offset += added;
        count -= added;
    }
}
auto LevelEventManager::add_spawn_event(IndexSpan const groups) -> EventHandle {
    auto const index{static_cast<std::uint32_t>(spawn_events_.size())};
    spawn_events_.push_back(groups);
    return EventHandle{index, EventKind::Spawn};
}
auto LevelEventManager::add_stop_event(RunEndReason const reason) -> EventHandle {
    auto const index{static_cast<std::uint32_t>(stop_events_.size())};
    stop_events_.push_back(reason);
    return EventHandle{index, EventKind::StopRun};
}
auto LevelEventManager::add_start_timer_event(StartTimerEvent const event) -> EventHandle {
    auto const index{static_cast<std::uint32_t>(start_timer_events_.size())};
    start_timer_events_.push_back(event);
    return EventHandle{index, EventKind::StartTimer};
}
auto LevelEventManager::add_win_event(Team const team) -> EventHandle {
    auto const index{static_cast<std::uint32_t>(win_events_.size())};
    win_events_.push_back({team});
    return EventHandle{index, EventKind::Win};
}
void LevelEventManager::add_trigger(ExprHandle const condition,
                                    std::span<EventHandle const> const events) {
    IndexSpan const range{static_cast<std::uint32_t>(trigger_events_.size()),
                          static_cast<std::uint32_t>(events.size())};
    trigger_events_.insert(trigger_events_.end(), events.begin(), events.end());
    active_triggers_.push_back(static_cast<TriggerIndex>(triggers_.size()));
    triggers_.push_back({condition, range});
}

/* **************************************** */
// Scheduling and dispatch
/* **************************************** */
void LevelEventManager::collect_ready(ConditionManager const& conditions) {
    auto const count{active_triggers_.size()};
    std::size_t retained{};
    for (std::size_t index{}; index < count; ++index) {
        auto const trigger_index{active_triggers_[index]};
        auto const trigger{triggers_[trigger_index]};
        if (!conditions.result(trigger.condition)) {
            active_triggers_[retained++] = trigger_index;
            continue;
        }
        auto const end{trigger.events.offset + trigger.events.count};
        for (auto event_index{trigger.events.offset}; event_index < end; ++event_index) {
            enqueue(trigger_events_[event_index]);
        }
    }
    // Preserve registrations after the pass boundary for the following pass.
    active_triggers_.erase(active_triggers_.begin() + static_cast<std::ptrdiff_t>(retained),
                           active_triggers_.begin() + static_cast<std::ptrdiff_t>(count));
}
void LevelEventManager::enqueue(EventHandle const event) {
    if (is_tick_end(event.kind())) {
        tick_end_events_.push_back(event);
    } else {
        require(is_preparation(event.kind()), "Event kind must belong to a dispatch phase");
        preparation_events_.push_back(event);
    }
}
void LevelEventManager::enqueue_stop(RunEndReason const reason) {
    enqueue(add_stop_event(reason));
}

auto LevelEventManager::consume_preparation_event() -> std::optional<EventHandle> {
    if (preparation_read_cursor_ == preparation_events_.size()) {
        return std::nullopt;
    }

    return preparation_events_[preparation_read_cursor_++];
}
auto LevelEventManager::consume_tick_end_event() -> std::optional<EventHandle> {
    if (tick_end_read_cursor_ == tick_end_events_.size()) {
        return std::nullopt;
    }

    return tick_end_events_[tick_end_read_cursor_++];
}

void LevelEventManager::dispatch_tick_end(Timers& timers,
                                          SimClock const& clock,
                                          LevelState& state,
                                          std::optional<TerminalEvent>& terminal) {
    if (state == LevelState::Stopped) {
        return;
    }
    while (auto const event = consume_tick_end_event()) {
        switch (event->kind()) {
            case EventKind::StartTimer: {
                auto const start{start_timer_event(*event)};
                timers.start(start.timer, start.duration);
                break;
            }
            case EventKind::Win:
            case EventKind::StopRun:
                terminal = TerminalEvent{*event, clock.get_completed_ticks()};
                state = LevelState::Stopped;
                return;
            case EventKind::Pause:
                state = LevelState::Paused;
                break;
            default:
                fatal_error("Invalid tick-end event kind");
        }
    }
}
void LevelEventManager::dispatch_preparation(LevelSpawnManager& spawns,
                                             LevelSpawnContext const& context) {
    while (auto const event = consume_preparation_event()) {
        switch (event->kind()) {
            case EventKind::Spawn:
                spawns.process_event(*event, *this, context);
                break;
            default:
                fatal_error("Invalid Preparation event kind");
        }
    }
}

}
