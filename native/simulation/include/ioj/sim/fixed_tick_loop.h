#pragma once

#include "ioj/sim/fixed_tick.h"
#include <ioj/constrained_numbers.h>
#include <ioj/frequency.h>

namespace ioj::sim {
struct FixedTickLoop {
    void initialise() { accumulator = 0.0; }

    [[nodiscard]] auto get_tick_period() const noexcept -> double {
        return tick_loop_period(tick_rate.get_hz());
    }

    void add_time(double const dt) { add_tick_loop_time(dt, time_scale, accumulator); }

    auto try_tick() -> bool { return try_consume_tick(get_tick_period(), accumulator); }

    Frequency tick_rate{60.0};
    PositiveFinite<double> time_scale{1.0};
    double accumulator{};
};
}
