#pragma once

#include <SpaceGame/support/FixedTickLoop.h>

struct SPACEGAME_API FSimulationClock {
    using tick_type = uint64;
    using time_type = double;

    void initialise(FFixedTickLoop const& settings) {
        tick_loop = settings;
        tick_loop.initialise();
        completed_ticks = 0;
    }

    auto frequency_to_tick_period(time_type frequency) const noexcept -> tick_type {
        check(frequency > 0.0);
        return static_cast<tick_type>(FMath::CeilToInt64(tick_loop.tick_rate / frequency));
    }
    auto duration_to_tick_period(time_type duration) const noexcept -> tick_type {
        check(duration >= 0.0);
        return static_cast<tick_type>(FMath::CeilToInt64(duration * tick_loop.tick_rate));
    }
    auto get_completed_ticks() const noexcept -> tick_type { return completed_ticks; }
    auto get_simulation_time() const noexcept -> time_type {
        return completed_ticks * tick_loop.tick_period;
    }
    auto get_tick_period() const noexcept -> time_type { return tick_loop.tick_period; }

    FFixedTickLoop tick_loop{};
    tick_type completed_ticks{};
};
