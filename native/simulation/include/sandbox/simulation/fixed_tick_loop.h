#pragma once

#include "sandbox/simulation/fixed_tick.h"

#include <cassert>

namespace ml::simulation {
struct FixedTickLoop {
    void initialise() {
        [[maybe_unused]] auto const succeeded{
            initialise_tick_loop(tick_rate, time_scale, tick_period, accumulator)};
        assert(succeeded);
    }

    void add_time(double const dt) { add_tick_loop_time(dt, time_scale, accumulator); }

    auto try_tick() -> bool { return try_consume_tick(tick_period, accumulator); }

    double tick_rate{60.0};
    double time_scale{1.0};
    double tick_period{};
    double accumulator{};
};
}
