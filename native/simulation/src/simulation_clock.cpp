#include "sandbox/simulation/simulation_clock.h"

#include <cmath>

namespace ml::simulation {
auto frequency_to_tick_period(double const tick_rate, double const frequency) noexcept
    -> std::uint64_t {
    return static_cast<std::uint64_t>(std::ceil(tick_rate / frequency));
}

auto duration_to_tick_period(double const tick_rate, double const duration) noexcept
    -> std::uint64_t {
    return static_cast<std::uint64_t>(std::ceil(duration * tick_rate));
}

auto simulation_time(std::uint64_t const completed_ticks, double const tick_period) noexcept
    -> double {
    return static_cast<double>(completed_ticks) * tick_period;
}
}
