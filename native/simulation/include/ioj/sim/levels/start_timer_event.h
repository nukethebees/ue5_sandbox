#pragma once

#include <ioj/sim/expressions/timer_handle.h>
#include <ioj/sim/sim_tick.h>

namespace ioj::sim {
struct StartTimerEvent {
    TimerHandle timer;
    SimTick duration;
};
}
