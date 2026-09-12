#include <sandbox/core/timing.h>

#include <gtest/gtest.h>

TEST(NativeCoreTiming, AcceptsPositivePeriods) {
    EXPECT_TRUE(ml::valid_periods(1, 2, 3));
    EXPECT_TRUE(ml::valid_periods(0.5f, 1.0, 2.5));
    EXPECT_TRUE(ml::valid_periods(1, 0.5f, 2.0));
}

TEST(NativeCoreTiming, RejectsNonPositivePeriods) {
    EXPECT_FALSE(ml::valid_periods(1, 0, 2));
    EXPECT_FALSE(ml::valid_periods(1.0, -0.5f, 2));
}
