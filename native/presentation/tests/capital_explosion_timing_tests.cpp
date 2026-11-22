#include <ioj/presentation/capital_explosion_timing.h>

#include <gtest/gtest.h>

namespace ioj::presentation {
TEST(CapitalExplosionTiming, EachCapitalUsesItsOwnLastSmallExplosion) {
    CapitalExplosionTiming const first{0.f, 0.25f, 3};
    CapitalExplosionTiming const second{first.last_small_delay(), 0.25f, 3};
    EXPECT_FLOAT_EQ(first.main_delay(2.f, true), 2.5f);
    EXPECT_FLOAT_EQ(second.small_delay(0), 0.5f);
    EXPECT_FLOAT_EQ(second.main_delay(2.f, true), 3.f);
    EXPECT_FLOAT_EQ(second.main_delay(2.f, false), 2.f);

    CapitalExplosionTiming const no_small_effects{0.f, 0.25f, 0};
    EXPECT_FLOAT_EQ(no_small_effects.main_delay(2.f, true), 2.f);
}
}
