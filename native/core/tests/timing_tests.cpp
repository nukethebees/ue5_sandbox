#include <ioj/periodic_timer.h>
#include <ioj/timing.h>

#include <gtest/gtest.h>

TEST(NativeCoreTiming, AcceptsPositivePeriods) {
    EXPECT_TRUE(ioj::valid_periods(1, 2, 3));
    EXPECT_TRUE(ioj::valid_periods(0.5f, 1.0, 2.5));
    EXPECT_TRUE(ioj::valid_periods(1, 0.5f, 2.0));
}

TEST(NativeCoreTiming, RejectsNonPositivePeriods) {
    EXPECT_FALSE(ioj::valid_periods(1, 0, 2));
    EXPECT_FALSE(ioj::valid_periods(1.0, -0.5f, 2));
}

TEST(NativeCoreTiming, PeriodicTimerCoalescesMissedPeriodsWithoutDrifting) {
    ioj::PeriodicTimer timer{ioj::PositiveFinite<double>::make(0.5)};
    EXPECT_TRUE(timer.try_consume(0));
    EXPECT_FALSE(timer.try_consume(0));
    EXPECT_FALSE(timer.try_consume(0.49));
    EXPECT_TRUE(timer.try_consume(0.5));
    EXPECT_FALSE(timer.try_consume(0.5));
    EXPECT_TRUE(timer.try_consume(3.7));
    EXPECT_FALSE(timer.try_consume(3.9));
    EXPECT_TRUE(timer.try_consume(4));
    timer.reset();
    EXPECT_TRUE(timer.try_consume(0));
    EXPECT_FALSE(timer.try_consume(0.1));
}
