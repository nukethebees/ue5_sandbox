#pragma once

#include <ioj/sim/fixed_tick_loop.h>
#include <ioj/sim/sim_time.h>
#include <ioj/sim/simulation_phase.h>

#include <utility>

namespace ioj::sim {

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
        return simulation_time(completed_ticks, tick_loop.get_tick_period());
    }
    auto get_tick_rate() const noexcept -> time_type { return tick_loop.tick_rate; }
    auto get_tick_period() const noexcept -> time_type { return tick_loop.get_tick_period(); }
    auto get_time_scale() const noexcept -> time_type { return tick_loop.time_scale; }

    FixedTickLoop tick_loop{};
    tick_type completed_ticks{};
    [[nodiscard]] auto phase() const noexcept -> SimulationPhase { return phase_; }

    [[nodiscard]] static constexpr auto permits_transition(SimulationPhase const from,
                                                           SimulationPhase const to) noexcept
        -> bool {
        switch (from) {
            case SimulationPhase::Initialisation:
                return to == SimulationPhase::StableSetup;
            case SimulationPhase::StableSetup:
                return to == SimulationPhase::BetweenTicks;
            case SimulationPhase::BetweenTicks:
                return to == SimulationPhase::Preparation;
            case SimulationPhase::Preparation:
                return to == SimulationPhase::Thinking;
            case SimulationPhase::Thinking:
                return to == SimulationPhase::Action;
            case SimulationPhase::Action:
                return to == SimulationPhase::Resolution;
            case SimulationPhase::Resolution:
                return to == SimulationPhase::ResolutionCommit;
            case SimulationPhase::ResolutionCommit:
                return to == SimulationPhase::BetweenTicks;
        }
        return false;
    }

    void transition_to(SimulationPhase const next) noexcept {
        assert(permits_transition(phase_, next));
        phase_ = next;
    }

    auto permits_structural_mutation() const noexcept -> bool {
        return permits_preparation_mutation() || phase_ == SimulationPhase::ResolutionCommit;
    }
    [[nodiscard]] auto permits_lookup() const noexcept -> bool {
        // Keep lookup-valid phases below the generated boundary.
        return std::to_underlying(phase_) < std::to_underlying(SimulationPhase::Initialisation);
    }
    auto permits_preparation_mutation() const noexcept -> bool {
        return phase_ == SimulationPhase::Initialisation || phase_ == SimulationPhase::Preparation;
    }
  private:
    friend struct SimClockTestAccess;
    SimulationPhase phase_{SimulationPhase::Initialisation};
};
} // namespace ioj::sim
