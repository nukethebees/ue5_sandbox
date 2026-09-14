#pragma once

#include <ioj/sim/fixed_tick_loop.h>

#include <ioj/sim/sim_time.h>

namespace ioj::sim {

struct SimClock {
    using tick_type = ioj::sim::SimTick;
    using time_type = double;

    void initialise(ioj::sim::FixedTickLoop const& settings) {
        tick_loop = settings;
        tick_loop.initialise();
        completed_ticks = 0;
    }

    auto frequency_to_tick_period(time_type frequency) const noexcept -> tick_type {
        assert(frequency > 0.0);
        return ioj::sim::frequency_to_tick_period(tick_loop.tick_rate, frequency);
    }
    auto duration_to_tick_period(time_type duration) const noexcept -> tick_type {
        assert(duration >= 0.0);
        return ioj::sim::duration_to_tick_period(tick_loop.tick_rate, duration);
    }
    auto get_completed_ticks() const noexcept -> tick_type { return completed_ticks; }
    auto get_simulation_time() const noexcept -> time_type {
        return ioj::sim::simulation_time(completed_ticks, tick_loop.tick_period);
    }
    auto get_tick_rate() const noexcept -> time_type { return tick_loop.tick_rate; }
    auto get_tick_period() const noexcept -> time_type { return tick_loop.tick_period; }
    auto get_time_scale() const noexcept -> time_type { return tick_loop.time_scale; }

    ioj::sim::FixedTickLoop tick_loop{};
    tick_type completed_ticks{};
};
} // namespace ioj::sim
