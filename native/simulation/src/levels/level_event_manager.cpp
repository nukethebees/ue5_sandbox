#include <ioj/sim/levels/level_event_manager.h>

#include <ioj/diagnostics.h>
#include <ioj/sim/expressions/expr_manager.h>

#include <algorithm>
#include <utility>

namespace ioj::sim {
LevelEventManager::LevelEventManager(CompiledLevelEvents data, std::pmr::memory_resource& memory)
    : data_{std::move(data)}
    , preparation_events_{&memory}
    , tick_end_events_{&memory} {
    auto const tick_end_count{static_cast<std::size_t>(std::ranges::count_if(
        data_.trigger_events, [](EventHandle const event) { return is_tick_end(event.kind()); }))};
    tick_end_events_.reserve(tick_end_count + data_.initial_events.size() + 2);
    preparation_events_.reserve(data_.trigger_events.size() - tick_end_count);
    auto const initial_events{std::exchange(data_.initial_events, {})};
    for (auto const event : initial_events) {
        enqueue(event);
    }
}

void LevelEventManager::collect_ready(ExprManager const& expressions) {
    std::erase_if(data_.pending_triggers, [&](EventTrigger const& trigger) {
        if (!expressions.result(trigger.condition)) {
            return false;
        }
        auto const effects{
            std::span{data_.trigger_events}.subspan(trigger.events.offset, trigger.events.count)};
        for (auto const event : effects) {
            enqueue(event);
        }
        return true;
    });
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
    enqueue(data_.add_stop_event(reason));
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
}
