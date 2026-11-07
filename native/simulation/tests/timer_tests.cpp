#include <ioj/sim/expressions/timers.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

namespace ioj::sim::timer_tests {
TEST(Timers, RemainsStoppedUntilExplicitStartAndExpiresAtDuration) {
    static_assert(sizeof(TimerHandle) == sizeof(std::uint32_t));
    Timers timers;
    auto const zero{timers.add(SimTick{})};
    auto const delayed{timers.add(SimTick{10})};
    EXPECT_FALSE(timers.elapsed(zero, SimTick{100}));
    EXPECT_FALSE(timers.elapsed(delayed, SimTick{100}));

    timers.start(zero, SimTick{100});
    timers.start(delayed, SimTick{100});
    EXPECT_TRUE(timers.elapsed(zero, SimTick{100}));
    EXPECT_FALSE(timers.elapsed(delayed, SimTick{100}));
    EXPECT_FALSE(timers.elapsed(delayed, SimTick{109}));
    EXPECT_TRUE(timers.elapsed(delayed, SimTick{110}));
    EXPECT_TRUE(timers.elapsed(delayed, SimTick{200}));
}

TEST(Timers, RepeatedStartNeverChangesTheOriginalStart) {
    Timers timers;
    auto const handle{timers.add(SimTick{10})};
    timers.start(handle, SimTick{5});
    timers.start(handle, SimTick{10});
    EXPECT_FALSE(timers.elapsed(handle, SimTick{14}));
    EXPECT_TRUE(timers.elapsed(handle, SimTick{15}));

    timers.start(handle, SimTick{20});
    EXPECT_TRUE(timers.elapsed(handle, SimTick{20}));
}

TEST(Timers, FreshInstancesHaveIndependentStartState) {
    Timers first;
    auto const first_handle{first.add(SimTick{10})};
    Timers second;
    auto const second_handle{second.add(SimTick{10})};
    first.start(first_handle, SimTick{});
    EXPECT_TRUE(first.elapsed(first_handle, SimTick{10}));
    EXPECT_FALSE(second.elapsed(second_handle, SimTick{10}));

    second.start(second_handle, SimTick{10});
    EXPECT_FALSE(second.elapsed(second_handle, SimTick{19}));
    EXPECT_TRUE(second.elapsed(second_handle, SimTick{20}));
    EXPECT_TRUE(first.elapsed(first_handle, SimTick{20}));
}

TEST(Timers, DurationComparisonDoesNotOverflowNearTickLimit) {
    auto const last_tick{std::numeric_limits<SimTick::value_type>::max()};
    Timers timers;
    auto const reached{timers.add(SimTick{10})};
    auto const unreached{timers.add(SimTick{11})};
    timers.start(reached, SimTick{last_tick - 10});
    timers.start(unreached, SimTick{last_tick - 10});
    EXPECT_FALSE(timers.elapsed(reached, SimTick{last_tick - 1}));
    EXPECT_TRUE(timers.elapsed(reached, SimTick{last_tick}));
    EXPECT_FALSE(timers.elapsed(unreached, SimTick{last_tick}));
}
}
