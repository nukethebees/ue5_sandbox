#include <ioj/sim/expressions/timers.h>

#include <cassert>
#include <cstdint>

namespace ioj::sim {
auto Timers::add(SimTick const duration) -> TimerHandle {
    assert(durations_.size() < TimerHandle::invalid_value);
    TimerHandle const handle{static_cast<std::uint32_t>(durations_.size())};
    durations_.push_back(duration);
    started_at_.emplace_back();
    return handle;
}
void Timers::start(TimerHandle const handle, SimTick const tick) {
    assert(is_valid(handle));
    auto& started{started_at_[handle.index()]};
    if (!started) {
        started = tick;
    }
}
auto Timers::elapsed(TimerHandle const handle, SimTick const tick) const -> bool {
    assert(is_valid(handle));
    auto const started{started_at_[handle.index()]};
    if (!started) {
        return false;
    }

    assert(tick >= *started);
    return tick.value() - started->value() >= durations_[handle.index()].value();
}
auto Timers::is_valid(TimerHandle const handle) const -> bool {
    return handle.is_valid() && handle.index() < durations_.size();
}
}
