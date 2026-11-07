#pragma once

#include <ioj/sim/expressions/detail/timer_entry.h>
#include <ioj/sim/expressions/timer_handle.h>
#include <ioj/sim/sim_tick.h>

#include <vector>

namespace ioj::sim {
class Timers {
  public:
    auto add() -> TimerHandle;
    void start(TimerHandle handle, SimTick duration);
    void tick(SimTick ticks);
    auto elapsed(TimerHandle handle) const -> bool;

  private:
    auto is_valid(TimerHandle handle) const -> bool;

    std::vector<detail::TimerEntry> entries_;

    std::vector<SimTick::value_type> remaining_ticks_;
    std::vector<TimerHandle> owners_;
};
}
