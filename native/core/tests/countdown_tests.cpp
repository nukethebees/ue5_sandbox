#include "sandbox/core/countdown.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

TEST(Countdown, DecrementsAndPeriodicallyClamps) {
    std::array<std::int8_t, 3> counters{1, 0, -1};
    std::int8_t cleaner{1};

    ml::tick_countdowns<std::int8_t>(counters, cleaner, 2);

    EXPECT_EQ(counters, (std::array<std::int8_t, 3>{0, 0, 0}));
    EXPECT_EQ(cleaner, 0);
}

TEST(Countdown, PeriodicCountdownSaturatesAtZero) {
    std::array<std::int16_t, 3> counters{2, 5, 0};

    ml::tick_periodic_countdowns<std::int16_t>(counters, 3);

    EXPECT_EQ(counters, (std::array<std::int16_t, 3>{0, 2, 0}));
}

TEST(Countdown, RestartsReadyCounters) {
    std::array<std::int32_t, 4> counters{-1, 0, 1, 2};

    ml::consume_ready_countdowns<std::int32_t>(counters, 10);

    EXPECT_EQ(counters, (std::array<std::int32_t, 4>{10, 10, 1, 2}));
}

TEST(Countdown, DecrementsFloatingPointTimers) {
    std::array counters{2.0f, 1.0f};

    ml::tick_countdowns(counters, 0.25f);

    EXPECT_EQ(counters, (std::array{1.75f, 0.75f}));
}
