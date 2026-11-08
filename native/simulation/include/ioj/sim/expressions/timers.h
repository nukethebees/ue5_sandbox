#pragma once

#include <ioj/sim/expressions/detail/timer_entry.h>
#include <ioj/sim/expressions/timer_handle.h>
#include <ioj/sim/expressions/timer_ticks.h>

#include <vector>

namespace ioj::sim {
class Timers {
  public:
    auto add() -> TimerHandle;
    auto add(std::uint32_t count) -> std::vector<TimerHandle>;
    void start(TimerHandle handle, TimerTicks duration);
    void tick(TimerTicks ticks);
    auto elapsed(TimerHandle handle) const -> bool;

  private:
    auto is_valid(TimerHandle handle) const -> bool;

    std::vector<detail::TimerEntry> entries_;

    std::vector<TimerTicks> remaining_ticks_;
    std::vector<TimerHandle> owners_;
};
}
