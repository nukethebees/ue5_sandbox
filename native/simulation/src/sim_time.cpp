#include "ioj/sim/sim_time.h"

#include <cmath>

namespace ioj::sim {
auto frequency_to_tick_period(double const tick_rate, double const frequency) noexcept -> SimTick {
    return static_cast<SimTick>(std::ceil(tick_rate / frequency));
}

auto duration_to_tick_period(double const tick_rate, double const duration) noexcept -> SimTick {
    return static_cast<SimTick>(std::ceil(duration * tick_rate));
}

auto simulation_time(SimTick const completed_ticks, double const tick_period) noexcept -> double {
    return static_cast<double>(completed_ticks) * tick_period;
}
}
