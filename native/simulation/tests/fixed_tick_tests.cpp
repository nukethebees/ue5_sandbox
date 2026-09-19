#include "ioj/sim/fixed_tick.h"
#include "ioj/sim/fixed_tick_loop.h"
#include "ioj/sim/sim_time.h"

#include <gtest/gtest.h>

#include <limits>

namespace ioj::sim::tests {

TEST(FixedTick, InitialisesDerivedPeriodAndResetsAccumulator) {
    FixedTickLoop loop{.tick_rate = 10.0, .time_scale = 2.0, .accumulator = 1.0};

    loop.initialise();

    EXPECT_DOUBLE_EQ(loop.get_tick_period(), 0.1);
    EXPECT_DOUBLE_EQ(loop.accumulator, 0.0);
}

TEST(FixedTick, AccumulatesAndConsumesScaledTime) {
    FixedTickLoop loop{.tick_rate = 10.0, .time_scale = 2.0};
    loop.initialise();

    loop.add_time(0.03);
    EXPECT_FALSE(loop.try_tick());
    loop.add_time(0.03);
    EXPECT_TRUE(loop.try_tick());
    EXPECT_NEAR(loop.accumulator, 0.02, 1e-12);
}

TEST(FixedTick, MakesExactlyOneTickAvailable) {
    FixedTickLoop loop{.tick_rate = 10.0, .time_scale = 2.0};
    loop.initialise();

    loop.add_time(0.05);
    EXPECT_TRUE(loop.try_tick());
    EXPECT_FALSE(loop.try_tick());
    EXPECT_DOUBLE_EQ(loop.accumulator, 0.0);
}

TEST(FixedTick, ConsumesMultipleAccumulatedTicks) {
    FixedTickLoop loop{.tick_rate = 10.0};
    loop.initialise();

    loop.add_time(0.35);
    EXPECT_TRUE(loop.try_tick());
    EXPECT_TRUE(loop.try_tick());
    EXPECT_TRUE(loop.try_tick());
    EXPECT_FALSE(loop.try_tick());
    EXPECT_NEAR(loop.accumulator, 0.05, 1e-12);
}

TEST(FixedTick, RejectsInvalidSettingsWithoutChangingAccumulator) {
    double accumulator{1.0};
    EXPECT_FALSE(initialise_tick_loop(0.0, 1.0, accumulator));
    EXPECT_FALSE(initialise_tick_loop(1.0, 0.0, accumulator));
    EXPECT_DOUBLE_EQ(accumulator, 1.0);
}

TEST(FixedTick, RetainsExistingNonFiniteInitialisationSemantics) {
    double accumulator{1.0};
    EXPECT_TRUE(initialise_tick_loop(std::numeric_limits<double>::quiet_NaN(), 1.0, accumulator));
    EXPECT_DOUBLE_EQ(accumulator, 0.0);
}

TEST(SimulationClock, ConvertsDurationsAndFrequencies) {
    EXPECT_EQ(frequency_to_tick_period(60.0, 7.0), 9);
    EXPECT_EQ(duration_to_tick_period(60.0, 0.11), 7);
    EXPECT_DOUBLE_EQ(simulation_time(12, 0.25), 3.0);
}

} // namespace tests
