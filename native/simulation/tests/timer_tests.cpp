#include <ioj/sim/expressions/timers.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

namespace ioj::sim::timer_tests {
TEST(Timers, RemainsStoppedUntilExplicitStartAndExpiresAtDuration) {
    static_assert(sizeof(TimerHandle) == sizeof(std::uint32_t));
    static_assert(sizeof(detail::TimerEntry) == sizeof(std::uint32_t));
    Timers timers;
    auto const zero{timers.add(SimTick{})};
    auto const delayed{timers.add(SimTick{10})};
    timers.tick(SimTick{100});
    EXPECT_FALSE(timers.elapsed(zero));
    EXPECT_FALSE(timers.elapsed(delayed));

    timers.start(zero);
    timers.start(delayed);
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
    auto const handle{timers.add(SimTick{10})};
    timers.start(handle);
    timers.tick(SimTick{5});
    timers.start(handle);
    timers.tick(SimTick{4});
    EXPECT_FALSE(timers.elapsed(handle));
    timers.tick(SimTick{1});
    EXPECT_TRUE(timers.elapsed(handle));

    timers.start(handle);
    timers.tick(SimTick{1});
    EXPECT_TRUE(timers.elapsed(handle));
}

TEST(Timers, FreshInstancesHaveIndependentStartState) {
    Timers first;
    auto const first_handle{first.add(SimTick{10})};
    Timers second;
    auto const second_handle{second.add(SimTick{10})};
    first.start(first_handle);
    first.tick(SimTick{10});
    second.tick(SimTick{10});
    EXPECT_TRUE(first.elapsed(first_handle));
    EXPECT_FALSE(second.elapsed(second_handle));

    second.start(second_handle);
    second.tick(SimTick{9});
    EXPECT_FALSE(second.elapsed(second_handle));
    second.tick(SimTick{1});
    EXPECT_TRUE(second.elapsed(second_handle));
    EXPECT_TRUE(first.elapsed(first_handle));
}

TEST(Timers, CountdownSupportsFullTickRangeAndSaturatesAtZero) {
    auto const max_ticks{std::numeric_limits<SimTick::value_type>::max()};
    Timers timers;
    auto const short_timer{timers.add(SimTick{10})};
    auto const long_timer{timers.add(SimTick{max_ticks})};
    timers.start(short_timer);
    timers.start(long_timer);
    timers.tick(SimTick{max_ticks - 1});
    EXPECT_TRUE(timers.elapsed(short_timer));
    EXPECT_FALSE(timers.elapsed(long_timer));
    timers.tick(SimTick{2});
    EXPECT_TRUE(timers.elapsed(long_timer));
}

TEST(Timers, ExpiryCompactsCountdownsWithoutChangingHandlesOrTickingMovedTimersTwice) {
    Timers timers;
    auto const first{timers.add(SimTick{1})};
    auto const second{timers.add(SimTick{1})};
    auto const moved{timers.add(SimTick{3})};
    auto const stopped{timers.add(SimTick{1})};
    timers.start(first);
    timers.start(second);
    timers.start(moved);

    timers.tick(SimTick{1});
    EXPECT_TRUE(timers.elapsed(first));
    EXPECT_TRUE(timers.elapsed(second));
    EXPECT_FALSE(timers.elapsed(moved));
    EXPECT_FALSE(timers.elapsed(stopped));

    timers.start(first);
    timers.start(moved);
    timers.start(stopped);
    timers.tick(SimTick{1});
    EXPECT_TRUE(timers.elapsed(stopped));
    EXPECT_FALSE(timers.elapsed(moved));
    timers.tick(SimTick{1});
    EXPECT_TRUE(timers.elapsed(moved));
    EXPECT_TRUE(timers.elapsed(first));
    EXPECT_TRUE(timers.elapsed(second));
}
}
