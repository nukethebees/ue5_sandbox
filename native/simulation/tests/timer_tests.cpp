#include <ioj/sim/expressions/timers.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

namespace ioj::sim::timer_tests {
TEST(Timers, RemainsStoppedUntilExplicitStartAndExpiresAtDuration) {
    static_assert(sizeof(TimerHandle) == sizeof(std::uint32_t));
    static_assert(sizeof(detail::TimerEntry) == sizeof(std::uint32_t));
    Timers timers;
    auto const handles{timers.add(2)};
    auto const zero{handles[0]};
    auto const delayed{handles[1]};
    timers.tick(100);
    EXPECT_FALSE(timers.elapsed(zero));
    EXPECT_FALSE(timers.elapsed(delayed));

    timers.start(zero, 0);
    timers.start(delayed, 10);
    EXPECT_FALSE(timers.elapsed(zero));
    timers.tick(0);
    EXPECT_TRUE(timers.elapsed(zero));
    EXPECT_FALSE(timers.elapsed(delayed));
    timers.tick(9);
    EXPECT_FALSE(timers.elapsed(delayed));
    timers.tick(1);
    EXPECT_TRUE(timers.elapsed(delayed));
    timers.tick(100);
    EXPECT_TRUE(timers.elapsed(delayed));
}

TEST(Timers, RepeatedStartDoesNotRestartRunningOrFinishedTimers) {
    Timers timers;
    auto const handle{timers.add()};
    timers.start(handle, 10);
    timers.tick(5);
    timers.start(handle, 100);
    timers.tick(4);
    EXPECT_FALSE(timers.elapsed(handle));
    timers.tick(1);
    EXPECT_TRUE(timers.elapsed(handle));

    timers.start(handle, 100);
    timers.tick(1);
    EXPECT_TRUE(timers.elapsed(handle));
}

TEST(Timers, FreshInstancesHaveIndependentStartState) {
    Timers first;
    auto const first_handle{first.add()};
    Timers second;
    auto const second_handle{second.add()};
    first.start(first_handle, 10);
    first.tick(10);
    second.tick(10);
    EXPECT_TRUE(first.elapsed(first_handle));
    EXPECT_FALSE(second.elapsed(second_handle));

    second.start(second_handle, 10);
    second.tick(9);
    EXPECT_FALSE(second.elapsed(second_handle));
    second.tick(1);
    EXPECT_TRUE(second.elapsed(second_handle));
    EXPECT_TRUE(first.elapsed(first_handle));
}

TEST(Timers, ExpiryCompactsCountdownsWithoutChangingHandlesOrTickingMovedTimersTwice) {
    Timers timers;
    auto const first{timers.add()};
    auto const second{timers.add()};
    auto const moved{timers.add()};
    auto const stopped{timers.add()};
    timers.start(first, 1);
    timers.start(second, 1);
    timers.start(moved, 3);

    timers.tick(1);
    EXPECT_TRUE(timers.elapsed(first));
    EXPECT_TRUE(timers.elapsed(second));
    EXPECT_FALSE(timers.elapsed(moved));
    EXPECT_FALSE(timers.elapsed(stopped));

    timers.start(first, 1);
    timers.start(moved, 3);
    timers.start(stopped, 1);
    timers.tick(1);
    EXPECT_TRUE(timers.elapsed(stopped));
    EXPECT_FALSE(timers.elapsed(moved));
    timers.tick(1);
    EXPECT_TRUE(timers.elapsed(moved));
    EXPECT_TRUE(timers.elapsed(first));
    EXPECT_TRUE(timers.elapsed(second));
}
}
