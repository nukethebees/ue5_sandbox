#include "ioj/sim/fixed_tick.h"

namespace ioj::sim {
auto tick_loop_period(double const tick_rate) noexcept -> double {
    return 1.0 / tick_rate;
}

void add_tick_loop_time(double const dt, double const time_scale, double& accumulator) noexcept {
    accumulator += dt * time_scale;
}

auto try_consume_tick(double const tick_period, double& accumulator) noexcept -> bool {
    if (accumulator < tick_period) {
        return false;
    }

    accumulator -= tick_period;
    return true;
}
}
