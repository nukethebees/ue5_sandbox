#pragma once

#include "sandbox/simulation/sim_tick.h"

namespace ml::simulation {
[[nodiscard]] auto frequency_to_tick_period(double tick_rate, double frequency) noexcept -> SimTick;
[[nodiscard]] auto duration_to_tick_period(double tick_rate, double duration) noexcept -> SimTick;
[[nodiscard]] auto simulation_time(SimTick completed_ticks, double tick_period) noexcept -> double;
}
