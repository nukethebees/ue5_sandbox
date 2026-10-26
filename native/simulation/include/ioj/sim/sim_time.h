#pragma once

#include "ioj/sim/sim_tick.h"
#include <ioj/constrained_numbers.h>
#include <ioj/duration.h>
#include <ioj/frequency.h>

#include <cmath>
#include <concepts>
#include <limits>

namespace ioj::sim {
using SimTime = double;

// Require a rounded period representable by Counter. Check before converting to an integer.
template <std::integral Counter = SimTick::value_type>
    requires (!std::same_as<Counter, bool>)
[[nodiscard]] auto frequency_to_tick_period(Frequency const tick_rate,
                                            Frequency const frequency) noexcept
    -> Positive<Counter> {
    auto const period{std::ceil(tick_rate.get_hz() / frequency.get_hz())};
    auto const upper_bound{std::ldexp(1.0, std::numeric_limits<Counter>::digits)};
    require(std::isfinite(period) && period >= 1.0 && period < upper_bound,
            "Frequency tick period must fit the countdown type");
    return Positive<Counter>::make(static_cast<Counter>(period));
}

// Allow zero duration. Require a rounded period representable by Counter.
template <std::integral Counter = SimTick::value_type>
    requires (!std::same_as<Counter, bool>)
[[nodiscard]] auto duration_to_tick_period(Frequency const tick_rate,
                                           Duration const duration) noexcept
    -> NonNegative<Counter> {
    auto const period{std::ceil(duration.get_seconds() * tick_rate.get_hz())};
    auto const upper_bound{std::ldexp(1.0, std::numeric_limits<Counter>::digits)};
    require(std::isfinite(period) && period >= 0.0 && period < upper_bound,
            "Duration tick period must fit the countdown type");
    return NonNegative<Counter>::make(static_cast<Counter>(period));
}

[[nodiscard]] auto simulation_time(SimTick completed_ticks, double tick_period) noexcept -> double;
}
