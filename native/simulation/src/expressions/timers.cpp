#include <ioj/sim/expressions/timers.h>

#include <algorithm>
#include <cassert>
#include <cstdint>

namespace ioj::sim {
auto Timers::add() -> TimerHandle {
    assert(entries_.size() < TimerHandle::invalid_value);
    TimerHandle const handle{static_cast<std::uint32_t>(entries_.size())};
    entries_.emplace_back();
    return handle;
}
void Timers::start(TimerHandle const handle, SimTick const duration) {
    assert(is_valid(handle));
    auto& entry{entries_[handle.index()]};
    if (entry.state() != detail::TimerState::NotStarted) {
        return;
    }

    assert(owners_.size() <= detail::TimerEntry::index_layout.range.max);
    entry =
        detail::TimerEntry{detail::TimerState::Started, static_cast<std::uint32_t>(owners_.size())};
    remaining_ticks_.push_back(duration.value());
    owners_.push_back(handle);
}
void Timers::tick(SimTick const ticks) {
    for (auto index{owners_.size()}; index-- > 0;) {
        auto& remaining{remaining_ticks_[index]};
        remaining -= std::min(remaining, ticks.value());
        if (remaining != 0) {
            continue;
        }

        entries_[owners_[index].index()].set_state(detail::TimerState::Finished);
        auto const last{owners_.size() - 1};
        if (index != last) {
            remaining_ticks_[index] = remaining_ticks_[last];
            owners_[index] = owners_[last];
            entries_[owners_[index].index()].set_index(static_cast<std::uint32_t>(index));
        }
        remaining_ticks_.pop_back();
        owners_.pop_back();
    }
}
auto Timers::elapsed(TimerHandle const handle) const -> bool {
    assert(is_valid(handle));
    return entries_[handle.index()].state() == detail::TimerState::Finished;
}
auto Timers::is_valid(TimerHandle const handle) const -> bool {
    return handle.is_valid() && handle.index() < entries_.size();
}
}
