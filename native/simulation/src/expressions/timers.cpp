#include <ioj/sim/expressions/timers.h>

#include <cassert>
#include <cstdint>

namespace ioj::sim {
auto Timers::add() -> TimerHandle {
    assert(states_.size() < TimerHandle::invalid_value);
    TimerHandle const handle{static_cast<std::uint32_t>(states_.size())};
    states_.push_back(detail::TimerState::NotStarted);
    return handle;
}
auto Timers::add(std::uint32_t const count) -> std::vector<TimerHandle> {
    auto const first{states_.size()};
    assert(count <= TimerHandle::invalid_value - first);
    states_.resize(first + count, detail::TimerState::NotStarted);
    std::vector<TimerHandle> handles;
    handles.reserve(count);
    for (std::uint32_t index{}; index < count; ++index) {
        handles.emplace_back(static_cast<std::uint32_t>(first) + index);
    }
    return handles;
}
void Timers::start(TimerHandle const handle, TimerTicks const duration) {
    assert(is_valid(handle));
    assert(duration >= 0);
    auto& state{states_[handle.index()]};
    if (state != detail::TimerState::NotStarted) {
        return;
    }

    state = detail::TimerState::Started;
    remaining_ticks_.push_back(duration);
    owners_.push_back(handle);
}
void Timers::tick(TimerTicks const ticks) {
    assert(ticks >= 0);
    for (auto index{owners_.size()}; index-- > 0;) {
        auto& remaining{remaining_ticks_[index]};
        remaining -= ticks;
        if (remaining > 0) {
            continue;
        }

        states_[owners_[index].index()] = detail::TimerState::Finished;
        auto const last{owners_.size() - 1};
        if (index != last) {
            remaining_ticks_[index] = remaining_ticks_[last];
            owners_[index] = owners_[last];
        }
        remaining_ticks_.pop_back();
        owners_.pop_back();
    }
}
auto Timers::elapsed(TimerHandle const handle) const -> bool {
    assert(is_valid(handle));
    return states_[handle.index()] == detail::TimerState::Finished;
}
auto Timers::is_valid(TimerHandle const handle) const -> bool {
    return handle.is_valid() && handle.index() < states_.size();
}
}
