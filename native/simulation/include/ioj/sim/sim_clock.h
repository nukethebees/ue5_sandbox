#pragma once

#include <ioj/settings/tick_settings.h>
#include <ioj/sim/fixed_tick_loop.h>
#include <ioj/sim/sim_time.h>
#include <ioj/sim/simulation_phase.h>

#include <cassert>
#include <utility>

namespace ioj::sim {

struct SimClock {
    using tick_type = SimTick;
    using time_type = SimTime;

    void initialise(settings::TickSettings const& settings) {
        tick_loop = {.tick_rate = settings.tick_rate, .time_scale = settings.time_scale};
        completed_ticks = 0;
    }

    template <std::integral Counter = tick_type>
    auto frequency_to_tick_period(Frequency const frequency) const noexcept -> Positive<Counter> {
        return sim::frequency_to_tick_period<Counter>(tick_loop.tick_rate, frequency);
    }
    template <std::integral Counter = tick_type>
    auto duration_to_tick_period(Duration const duration) const noexcept -> NonNegative<Counter> {
        return sim::duration_to_tick_period<Counter>(tick_loop.tick_rate, duration);
    }
    auto get_completed_ticks() const noexcept -> tick_type { return completed_ticks; }
    auto get_simulation_time() const noexcept -> time_type {
        return simulation_time(completed_ticks, tick_loop.get_tick_period());
    }
    auto get_tick_rate() const noexcept -> Frequency { return tick_loop.tick_rate; }
    auto get_tick_period() const noexcept -> time_type { return tick_loop.get_tick_period(); }
    auto get_time_scale() const noexcept -> PositiveFinite<time_type> {
        return tick_loop.time_scale;
    }

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
