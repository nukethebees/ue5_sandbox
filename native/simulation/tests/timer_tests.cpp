#include <ioj/sim/expressions/timers.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

namespace ioj::sim::timer_tests {
TEST(Timers, RemainsStoppedUntilExplicitStartAndExpiresAtDuration) {
    static_assert(sizeof(TimerHandle) == sizeof(std::uint32_t));
    static_assert(sizeof(detail::TimerEntry) == sizeof(std::uint32_t));
    Timers timers;
    auto const zero{timers.add()};
    auto const delayed{timers.add()};
    timers.tick(SimTick{100});
    EXPECT_FALSE(timers.elapsed(zero));
    EXPECT_FALSE(timers.elapsed(delayed));

    timers.start(zero, SimTick{});
    timers.start(delayed, SimTick{10});
    EXPECT_FALSE(timers.elapsed(zero));
    timers.tick(SimTick{});
    EXPECT_TRUE(timers.elapsed(zero));
    EXPECT_FALSE(timers.elapsed(delayed));
    timers.tick(SimTick{9});
    EXPECT_FALSE(timers.elapsed(delayed));
    timers.tick(SimTick{1});
    EXPECT_TRUE(timers.elapsed(delayed));
    timers.tick(SimTick{100});
    EXPECT_TRUE(timers.elapsed(delayed));
}

TEST(Timers, RepeatedStartDoesNotRestartRunningOrFinishedTimers) {
    Timers timers;
    auto const handle{timers.add()};
    timers.start(handle, SimTick{10});
    timers.tick(SimTick{5});
    timers.start(handle, SimTick{100});
    timers.tick(SimTick{4});
    EXPECT_FALSE(timers.elapsed(handle));
    timers.tick(SimTick{1});
    EXPECT_TRUE(timers.elapsed(handle));

    timers.start(handle, SimTick{100});
    timers.tick(SimTick{1});
    EXPECT_TRUE(timers.elapsed(handle));
}

TEST(Timers, FreshInstancesHaveIndependentStartState) {
    Timers first;
    auto const first_handle{first.add()};
    Timers second;
    auto const second_handle{second.add()};
    first.start(first_handle, SimTick{10});
    first.tick(SimTick{10});
    second.tick(SimTick{10});
    EXPECT_TRUE(first.elapsed(first_handle));
    EXPECT_FALSE(second.elapsed(second_handle));

    second.start(second_handle, SimTick{10});
    second.tick(SimTick{9});
    EXPECT_FALSE(second.elapsed(second_handle));
    second.tick(SimTick{1});
    EXPECT_TRUE(second.elapsed(second_handle));
    EXPECT_TRUE(first.elapsed(first_handle));
}

TEST(Timers, CountdownSupportsFullTickRangeAndSaturatesAtZero) {
    auto const max_ticks{std::numeric_limits<SimTick::value_type>::max()};
    Timers timers;
    auto const short_timer{timers.add()};
    auto const long_timer{timers.add()};
    timers.start(short_timer, SimTick{10});
    timers.start(long_timer, SimTick{max_ticks});
    timers.tick(SimTick{max_ticks - 1});
    EXPECT_TRUE(timers.elapsed(short_timer));
    EXPECT_FALSE(timers.elapsed(long_timer));
    timers.tick(SimTick{2});
    EXPECT_TRUE(timers.elapsed(long_timer));
}

TEST(Timers, ExpiryCompactsCountdownsWithoutChangingHandlesOrTickingMovedTimersTwice) {
    Timers timers;
    auto const first{timers.add()};
    auto const second{timers.add()};
    auto const moved{timers.add()};
    auto const stopped{timers.add()};
    timers.start(first, SimTick{1});
    timers.start(second, SimTick{1});
    timers.start(moved, SimTick{3});

    timers.tick(SimTick{1});
    EXPECT_TRUE(timers.elapsed(first));
    EXPECT_TRUE(timers.elapsed(second));
    EXPECT_FALSE(timers.elapsed(moved));
    EXPECT_FALSE(timers.elapsed(stopped));

    timers.start(first, SimTick{1});
    timers.start(moved, SimTick{3});
    timers.start(stopped, SimTick{1});
    timers.tick(SimTick{1});
    EXPECT_TRUE(timers.elapsed(stopped));
    EXPECT_FALSE(timers.elapsed(moved));
    timers.tick(SimTick{1});
    EXPECT_TRUE(timers.elapsed(moved));
    EXPECT_TRUE(timers.elapsed(first));
    EXPECT_TRUE(timers.elapsed(second));
}
}
