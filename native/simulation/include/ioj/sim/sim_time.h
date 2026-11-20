#pragma once

#include "ioj/sim/sim_tick.h"
#include <ioj/constrained_numbers.h>
#include <ioj/duration.h>
#include <ioj/frequency.h>

#include <cmath>
#include <concepts>
#include <limits>
#include <optional>

namespace ioj::sim {
using SimTime = double;

// Require a rounded period representable by Counter. Check before converting to an integer.
template <std::integral Counter = SimTick::value_type>
    requires (!std::same_as<Counter, bool>)
[[nodiscard]] auto frequency_to_tick_period(Frequency const tick_rate,
                                            Frequency const frequency) noexcept
    -> Positive<Counter> {
    auto const period{std::ceil(tick_rate.hz() / frequency.hz())};
    auto const upper_bound{std::ldexp(1.0, std::numeric_limits<Counter>::digits)};
    require(period >= 1.0 && period < upper_bound,
            "Frequency tick period must fit the countdown type");
    return Positive<Counter>::make(static_cast<Counter>(period));
}

// Allow zero duration. Require a rounded period representable by Counter.
template <std::integral Counter = SimTick::value_type>
    requires (!std::same_as<Counter, bool>)
[[nodiscard]] auto try_duration_to_tick_period(Frequency const tick_rate,
                                               Duration const duration) noexcept
    -> std::optional<NonNegative<Counter>> {
    auto const period{std::ceil(duration.seconds() * tick_rate.hz())};
    auto const upper_bound{std::ldexp(1.0, std::numeric_limits<Counter>::digits)};
    if (period >= upper_bound) {
        return std::nullopt;
    }
    return NonNegative<Counter>::make(static_cast<Counter>(period));
}

template <std::integral Counter = SimTick::value_type>
    requires (!std::same_as<Counter, bool>)
[[nodiscard]] auto duration_to_tick_period(Frequency tick_rate, Duration duration) noexcept
    -> NonNegative<Counter> {
    auto const period{try_duration_to_tick_period<Counter>(tick_rate, duration)};
    require(period.has_value(), "Duration tick period must fit the countdown type");
    return *period;
}

[[nodiscard]] auto simulation_time(SimTick completed_ticks, double tick_period) noexcept -> double;
}
