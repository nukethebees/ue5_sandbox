#pragma once

#include <ioj/sim/expressions/timer_handle.h>
#include <ioj/sim/sim_tick.h>

#include <optional>
#include <vector>

namespace ioj::sim {
class Timers {
  public:
    auto add(SimTick duration) -> TimerHandle;
    void start(TimerHandle handle, SimTick tick);
    auto elapsed(TimerHandle handle, SimTick tick) const -> bool;

  private:
    auto is_valid(TimerHandle handle) const -> bool;

    std::vector<SimTick> durations_;
    std::vector<std::optional<SimTick>> started_at_;
};
}
