#pragma once

#include <ioj/sim/expressions/timer_handle.h>
#include <ioj/sim/expressions/timer_ticks.h>

namespace ioj::sim {
struct StartTimerEvent {
    TimerHandle timer;
    TimerTicks duration;
};
}
