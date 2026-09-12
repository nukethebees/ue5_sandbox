#include "sandbox/simulation/fixed_tick.h"

namespace ml::simulation {
auto initialise_tick_loop(double const tick_rate,
                          double const time_scale,
                          double& tick_period,
                          double& accumulator) noexcept -> bool {
    if (tick_rate <= 0.0 || time_scale <= 0.0) {
        return false;
    }

    tick_period = 1.0 / tick_rate;
    accumulator = 0.0;
    return true;
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
