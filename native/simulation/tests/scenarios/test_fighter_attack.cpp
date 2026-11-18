#include "../support/simulation_test_support.h"
#include <ioj/sim/fighters/sim.h>

namespace ioj::sim {
TEST(WorldlessSpaceGameSimulation, NavigationCountersFollowExecutedTicks) {
    auto const config{tests::make_fixture()};
    LevelSim simulation{tests::make_simulation_data(config)};

    fighters::NavigationCounters counters{.separation_query_count = 7, .hard_trace_count = 9};
    auto const tick_period{simulation.clock().tick_period()};

    simulation.advance(tick_period, &counters);
    EXPECT_EQ(counters.separation_query_count, 7);
    EXPECT_EQ(counters.hard_trace_count, 9);

    simulation.start();
    simulation.advance(0.0, &counters);
    EXPECT_EQ(counters.separation_query_count, 7);
    EXPECT_EQ(counters.hard_trace_count, 9);

    simulation.advance(tick_period * 2.5, &counters);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{2});
    EXPECT_EQ(counters.separation_query_count, 0);
    EXPECT_EQ(counters.hard_trace_count, 0);
}
}
