#include "sandbox/simulation/fixed_tick.h"
#include "sandbox/simulation/simulation_clock.h"

#include <gtest/gtest.h>

TEST(FixedTick, AccumulatesAndConsumesScaledTime) {
    double period{};
    double accumulator{};
    ASSERT_TRUE(ml::simulation::initialise_tick_loop(10.0, 2.0, period, accumulator));
    EXPECT_DOUBLE_EQ(period, 0.1);

    ml::simulation::add_tick_loop_time(0.03, 2.0, accumulator);
    EXPECT_FALSE(ml::simulation::try_consume_tick(period, accumulator));
    ml::simulation::add_tick_loop_time(0.03, 2.0, accumulator);
    EXPECT_TRUE(ml::simulation::try_consume_tick(period, accumulator));
    EXPECT_NEAR(accumulator, 0.02, 1e-12);
}

TEST(FixedTick, RejectsInvalidSettings) {
    double period{1.0};
    double accumulator{1.0};
    EXPECT_FALSE(ml::simulation::initialise_tick_loop(0.0, 1.0, period, accumulator));
    EXPECT_FALSE(ml::simulation::initialise_tick_loop(1.0, 0.0, period, accumulator));
    EXPECT_DOUBLE_EQ(period, 1.0);
    EXPECT_DOUBLE_EQ(accumulator, 1.0);
}

TEST(SimulationClock, ConvertsDurationsAndFrequencies) {
    EXPECT_EQ(ml::simulation::frequency_to_tick_period(60.0, 7.0), 9);
    EXPECT_EQ(ml::simulation::duration_to_tick_period(60.0, 0.11), 7);
    EXPECT_DOUBLE_EQ(ml::simulation::simulation_time(12, 0.25), 3.0);
}
