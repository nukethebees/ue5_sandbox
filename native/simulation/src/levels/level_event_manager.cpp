#include <ioj/sim/levels/level_event_manager.h>

#include <ioj/sim/expressions/expr_manager.h>

#include <algorithm>
#include <utility>

namespace ioj::sim {
LevelEventManager::LevelEventManager(CompiledLevelEvents data, std::pmr::memory_resource& memory)
    : data_{std::move(data)}
    , ready_events_{&memory} {
    ready_events_.reserve(data_.schedule.trigger_events.size() + 1);
}

void LevelEventManager::activate(ExprManager& expressions) {
    for (auto const& trigger : data_.schedule.pending_triggers) {
        expressions.activate(trigger.condition, SimTick{});
    }
}

void LevelEventManager::collect_ready(ExprManager& expressions) {
    auto& schedule{data_.schedule};
    std::erase_if(schedule.pending_triggers, [&](EventTrigger const& trigger) {
        auto const result{expressions.result(trigger.condition)};
        if (result == ExprResult::Pending) {
            return false;
        }

        if (result == ExprResult::True) {
            auto const events{std::span{schedule.trigger_events}.subspan(trigger.events.offset,
                                                                         trigger.events.count)};
            ready_events_.insert(ready_events_.end(), events.begin(), events.end());
        }
        expressions.deactivate(trigger.condition);
        return true;
    });
}

void LevelEventManager::enqueue(EventHandle const event) {
    ready_events_.push_back(event);
}
void LevelEventManager::clear_ready() {
    ready_events_.clear();
}
}
