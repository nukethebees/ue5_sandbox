#pragma once

#include <ioj/sim/fixed_tick_loop.h>

#include <ioj/sim/sim_time.h>

namespace ioj::sim {

enum class SimulationPhase { Initialisation, Preparation, Thinking, Action, Resolution, Idle };

struct SimClock {
    using tick_type = SimTick;
    using time_type = double;

    void initialise(FixedTickLoop const& settings) {
        tick_loop = settings;
        tick_loop.initialise();
        completed_ticks = 0;
    }

    auto frequency_to_tick_period(time_type frequency) const noexcept -> tick_type {
        assert(frequency > 0.0);
        return sim::frequency_to_tick_period(tick_loop.tick_rate, frequency);
    }
    auto duration_to_tick_period(time_type duration) const noexcept -> tick_type {
        assert(duration >= 0.0);
        return sim::duration_to_tick_period(tick_loop.tick_rate, duration);
    }
    auto get_completed_ticks() const noexcept -> tick_type { return completed_ticks; }
    auto get_simulation_time() const noexcept -> time_type {
        return simulation_time(completed_ticks, tick_loop.tick_period);
    }
    auto get_tick_rate() const noexcept -> time_type { return tick_loop.tick_rate; }
    auto get_tick_period() const noexcept -> time_type { return tick_loop.tick_period; }
    auto get_time_scale() const noexcept -> time_type { return tick_loop.time_scale; }

    FixedTickLoop tick_loop{};
    tick_type completed_ticks{};
    SimulationPhase phase{SimulationPhase::Initialisation};

    auto permits_structural_mutation() const noexcept -> bool {
        return phase == SimulationPhase::Initialisation || phase == SimulationPhase::Preparation;
    }
};
} // namespace ioj::sim
