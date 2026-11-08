#pragma once

#include <ioj/settings/tick_settings.h>
#include <ioj/sim/fixed_tick_loop.h>
#include <ioj/sim/sim_time.h>
#include <ioj/sim/simulation_phase.h>

#include <algorithm>
#include <cassert>
#include <utility>

namespace ioj::sim {

struct SimClock {
    using tick_type = SimTick;
    using time_type = SimTime;

    explicit SimClock(settings::TickSettings const& settings = {})
        : tick_loop_{.tick_rate = settings.tick_rate, .time_scale = settings.time_scale} {}

    void initialise(settings::TickSettings const& settings) {
        tick_loop_ = {.tick_rate = settings.tick_rate, .time_scale = settings.time_scale};
        completed_ticks_ = SimTick{};
        phase_ = SimulationPhase::Preparation;
    }

    // Finish setup by resetting tick progress while retaining the configured timing.
    void reset() noexcept {
        tick_loop_.initialise();
        completed_ticks_ = SimTick{};
        phase_ = SimulationPhase::BetweenTicks;
    }

    void add_time(time_type const dt) { tick_loop_.add_time(dt); }
    auto try_tick() -> bool { return tick_loop_.try_tick(); }
    void complete_tick() noexcept {
        assert(phase_ == SimulationPhase::ResolutionCommit);
        completed_ticks_.set_value(completed_ticks_.value() + 1);
        transition_to(SimulationPhase::BetweenTicks);
    }
    auto interpolation_alpha() const noexcept -> double {
        return std::clamp(tick_loop_.accumulator / tick_period(), 0.0, 1.0);
    }

    template <std::integral Counter = tick_type::value_type>
    auto frequency_to_tick_period(Frequency const frequency) const noexcept -> Positive<Counter> {
        return sim::frequency_to_tick_period<Counter>(tick_loop_.tick_rate, frequency);
    }
    template <std::integral Counter = tick_type::value_type>
    auto duration_to_tick_period(Duration const duration) const noexcept -> NonNegative<Counter> {
        return sim::duration_to_tick_period<Counter>(tick_loop_.tick_rate, duration);
    }
    auto get_completed_ticks() const noexcept -> tick_type { return completed_ticks_; }
    auto simulation_time() const noexcept -> time_type {
        return sim::simulation_time(completed_ticks_, tick_loop_.get_tick_period());
    }
    auto tick_rate() const noexcept -> Frequency { return tick_loop_.tick_rate; }
    auto tick_period() const noexcept -> time_type { return tick_loop_.get_tick_period(); }
    auto time_scale() const noexcept -> PositiveFinite<time_type> { return tick_loop_.time_scale; }

    [[nodiscard]] auto phase() const noexcept -> SimulationPhase { return phase_; }

    [[nodiscard]] static constexpr auto permits_transition(SimulationPhase const from,
                                                           SimulationPhase const to) noexcept
        -> bool {
        return cycle_next(from) == to;
    }

    void transition_to(SimulationPhase const next) noexcept {
        assert(permits_transition(phase_, next));
        phase_ = next;
    }

    // Advances phase state only; revisiting the current phase traverses a full cycle.
    void advance_to(SimulationPhase const next) noexcept {
        assert(std::to_underlying(next) <= std::to_underlying(SimulationPhase::BetweenTicks));

        do {
            phase_ = cycle_next(phase_);
        } while (phase_ != next);
    }

  private:
    FixedTickLoop tick_loop_{};
    tick_type completed_ticks_{};
    SimulationPhase phase_{SimulationPhase::Preparation};
};
} // namespace ioj::sim
