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
        completed_ticks = SimTick{};
        phase_ = SimulationPhase::Preparation;
    }

    // Finish setup by resetting tick progress while retaining the configured timing.
    void reset() noexcept {
        tick_loop.initialise();
        completed_ticks = SimTick{};
        phase_ = SimulationPhase::BetweenTicks;
    }

    template <std::integral Counter = tick_type::value_type>
    auto frequency_to_tick_period(Frequency const frequency) const noexcept -> Positive<Counter> {
        return sim::frequency_to_tick_period<Counter>(tick_loop.tick_rate, frequency);
    }
    template <std::integral Counter = tick_type::value_type>
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

    [[nodiscard]] auto phase() const noexcept -> SimulationPhase { return phase_; }

    [[nodiscard]] static constexpr auto permits_transition(SimulationPhase const from,
                                                           SimulationPhase const to) noexcept
        -> bool {
        return next_phase(from) == to;
    }

    void transition_to(SimulationPhase const next) noexcept {
        assert(permits_transition(phase_, next));
        phase_ = next;
    }

    // Advances phase state only; revisiting the current phase traverses a full cycle.
    void advance_to(SimulationPhase const next) noexcept {
        assert(std::to_underlying(next) <= std::to_underlying(SimulationPhase::BetweenTicks));

        do {
            phase_ = next_phase(phase_);
        } while (phase_ != next);
    }

  private:
    [[nodiscard]] static constexpr auto next_phase(SimulationPhase const phase) noexcept
        -> SimulationPhase {
        auto const value{std::to_underlying(phase)};
        assert(value <= std::to_underlying(SimulationPhase::BetweenTicks));
        return phase == SimulationPhase::BetweenTicks ? SimulationPhase::Preparation
                                                      : static_cast<SimulationPhase>(value + 1);
    }

  public:
    FixedTickLoop tick_loop{};
    tick_type completed_ticks{};

  private:
    SimulationPhase phase_{SimulationPhase::Preparation};
};
} // namespace ioj::sim
