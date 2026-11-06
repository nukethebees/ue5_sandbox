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
    std::array<levels::MissionExpr, 1> const condition{levels::MissionExpr{levels::TimeElapsedExpr{
        NonNegativeFinite<double>::make(simulation_time(trigger_tick, clock.tick_period()))}}};
    auto expression{data.expressions.append(condition, {{}, {}, clock})};
    ASSERT_TRUE(expression);

    auto& schedule{data.level_events.schedule};
    auto const first{schedule.spawn_groups.num()};
    schedule.add_spawn_group(type, offset, count);
    schedule.pending_spawns.push_back(
        {expression->handles[0], {first, schedule.spawn_groups.num() - first}});
}
}
