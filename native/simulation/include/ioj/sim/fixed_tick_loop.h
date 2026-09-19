#pragma once

#include "ioj/sim/fixed_tick.h"

#include <cassert>

namespace ioj::sim {
struct FixedTickLoop {
    void initialise() {
        [[maybe_unused]] auto const succeeded{
            initialise_tick_loop(tick_rate, time_scale, accumulator)};
        assert(succeeded);
    }

    [[nodiscard]] auto get_tick_period() const noexcept -> double {
        return tick_loop_period(tick_rate);
    }

    void add_time(double const dt) { add_tick_loop_time(dt, time_scale, accumulator); }

    auto try_tick() -> bool { return try_consume_tick(get_tick_period(), accumulator); }

    double tick_rate{60.0};
    double time_scale{1.0};
    double accumulator{};
};
}
