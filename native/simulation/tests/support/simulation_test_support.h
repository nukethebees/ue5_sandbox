#pragma once
#include "simulation_fixture.h"
#include "simulation_test_data.h"
#include "worldless_simulation_test.h"
#include <ioj/rotator_math.h>
#include <ioj/sim/collision/entity_world_bounds.h>
#include <ioj/sim/collision/line_traces.h>
#include <ioj/time_series_data.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <vector>

namespace ioj::sim::tests {
inline void add_timed_spawn(LevelSimInitData& data,
                            EntityType const type,
                            std::int32_t const offset,
                            std::int32_t const count,
                            SimTick const trigger_tick) {
    SimClock clock;
    clock.initialise(data.clock_settings);
    auto& schedule{data.level_events.schedule};
    auto const first{schedule.spawn_groups.num()};
    schedule.add_spawn_group(type, offset, count);
    auto const index{static_cast<std::uint32_t>(schedule.spawn_events.size())};
    schedule.spawn_events.push_back({first, schedule.spawn_groups.num() - first});
    std::array const events{EventHandle{index, EventKind::Spawn}};
    ASSERT_TRUE(schedule.add_timed_trigger(
        data.expressions,
        clock,
        Duration::make(simulation_time(trigger_tick, clock.tick_period())),
        events));
}
}
