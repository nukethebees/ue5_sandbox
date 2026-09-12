#pragma once

#include <SpaceGameSimulation/support/FixedTickLoop.h>

#include <sandbox/simulation/simulation_clock.h>

struct SPACEGAMESIMULATION_API FSimulationClock {
    using tick_type = uint64;
    using time_type = double;

    void initialise(FFixedTickLoop const& settings) {
        tick_loop = settings;
        tick_loop.initialise();
        completed_ticks = 0;
    }

    auto frequency_to_tick_period(time_type frequency) const noexcept -> tick_type {
        check(frequency > 0.0);
        return ml::simulation::frequency_to_tick_period(tick_loop.tick_rate, frequency);
    }
    auto duration_to_tick_period(time_type duration) const noexcept -> tick_type {
        check(duration >= 0.0);
        return ml::simulation::duration_to_tick_period(tick_loop.tick_rate, duration);
    }
    auto get_completed_ticks() const noexcept -> tick_type { return completed_ticks; }
    auto get_simulation_time() const noexcept -> time_type {
        return ml::simulation::simulation_time(completed_ticks, tick_loop.tick_period);
    }
    auto get_tick_rate() const noexcept -> time_type { return tick_loop.tick_rate; }
    auto get_tick_period() const noexcept -> time_type { return tick_loop.tick_period; }
    auto get_time_scale() const noexcept -> time_type { return tick_loop.time_scale; }

    FFixedTickLoop tick_loop{};
    tick_type completed_ticks{};
};
