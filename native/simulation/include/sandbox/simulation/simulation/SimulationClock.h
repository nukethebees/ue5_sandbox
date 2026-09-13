#pragma once

#include <sandbox/simulation/fixed_tick_loop.h>

#include <sandbox/simulation/simulation_clock.h>

struct FSimulationClock {
    using tick_type = ml::simulation::SimTick;
    using time_type = double;

    void initialise(ml::simulation::FixedTickLoop const& settings) {
        tick_loop = settings;
        tick_loop.initialise();
        completed_ticks = 0;
    }

    auto frequency_to_tick_period(time_type frequency) const noexcept -> tick_type {
        assert(frequency > 0.0);
        return ml::simulation::frequency_to_tick_period(tick_loop.tick_rate, frequency);
    }
    auto duration_to_tick_period(time_type duration) const noexcept -> tick_type {
        assert(duration >= 0.0);
        return ml::simulation::duration_to_tick_period(tick_loop.tick_rate, duration);
    }
    auto get_completed_ticks() const noexcept -> tick_type { return completed_ticks; }
    auto get_simulation_time() const noexcept -> time_type {
        return ml::simulation::simulation_time(completed_ticks, tick_loop.tick_period);
    }
    auto get_tick_rate() const noexcept -> time_type { return tick_loop.tick_rate; }
    auto get_tick_period() const noexcept -> time_type { return tick_loop.tick_period; }
    auto get_time_scale() const noexcept -> time_type { return tick_loop.time_scale; }

    ml::simulation::FixedTickLoop tick_loop{};
    tick_type completed_ticks{};
};
