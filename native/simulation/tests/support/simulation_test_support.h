#pragma once
#include "simulation_fixture.h"
#include "simulation_test_data.h"
#include "worldless_simulation_test.h"
#include <ioj/rotator_math.h>
#include <ioj/sim/collision/entity_world_bounds.h>
#include <ioj/sim/collision/line_traces.h>
#include <ioj/sim/levels/level_compilation.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <vector>

namespace ioj::sim::tests {
inline void add_timed_events(LevelSimInitData& data,
                             Duration const duration,
                             std::span<EventHandle const> const effects) {
    auto const ticks{levels::compile_timer_duration(duration, data.clock_settings.tick_rate)};
    ASSERT_TRUE(ticks);
    auto const timer{data.timers.add()};
    auto const elapsed{data.conditions.add_timer_elapsed(timer)};
    auto& events{data.level_events};
    events.enqueue(events.add_start_timer_event({timer, *ticks}));
    events.add_trigger(elapsed, effects);
}
inline void add_duration_limit(LevelSimInitData& data, Duration const duration) {
    std::array const effects{data.level_events.add_stop_event(RunEndReason::DurationReached)};
    add_timed_events(data, duration, effects);
}
}
