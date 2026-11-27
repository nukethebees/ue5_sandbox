#pragma once

#include <ioj/sim/expressions/detail/timer_state.h>
#include <ioj/sim/expressions/timer_handle.h>
#include <ioj/sim/expressions/timer_ticks.h>

#include <memory_resource>
#include <optional>
#include <vector>

namespace ioj::sim {
// Track level countdowns in simulation ticks.
class Timers {
  public:
    auto add() -> TimerHandle;
    auto add(std::uint32_t count, std::pmr::memory_resource& memory)
        -> std::pmr::vector<TimerHandle>;
    void start(TimerHandle handle, TimerTicks duration);
    auto start_run_limit(TimerTicks duration) -> TimerHandle;
    auto run_ticks_remaining() const -> std::optional<TimerTicks>;
    void tick(TimerTicks ticks);
    auto elapsed(TimerHandle handle) const -> bool;

  private:
    auto is_valid(TimerHandle handle) const -> bool;

    TimerHandle run_limit_;
    std::vector<detail::TimerState> states_;

    std::vector<TimerTicks> remaining_ticks_;
    std::vector<TimerHandle> owners_;
};
}
