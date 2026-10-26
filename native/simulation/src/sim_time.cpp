#include "ioj/sim/sim_time.h"

namespace ioj::sim {
auto simulation_time(SimTick const completed_ticks, double const tick_period) noexcept -> double {
    return static_cast<double>(completed_ticks.value()) * tick_period;
}
}
