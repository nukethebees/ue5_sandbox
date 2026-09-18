#include "sandbox/core/countdown.h"
#include "sandbox/core/countdown_timers.h"
#include "sandbox/core/periodic_tick_countdown.h"
#include "sandbox/core/tick_countdown.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace {
template <typename T>
auto copy_values(std::span<T> const values) -> std::vector<std::remove_const_t<T>> {
    return {values.begin(), values.end()};
}
}

TEST(NativeCountdown, DecrementsAndPeriodicallyClamps) {
    std::array<std::int8_t, 3> counters{1, 0, -1};
    std::int8_t cleaner{1};

    ml::tick_countdowns<std::int8_t>(counters, cleaner, 2);

    EXPECT_EQ(counters, (std::array<std::int8_t, 3>{0, 0, 0}));
    EXPECT_EQ(cleaner, 0);
}

TEST(NativeCountdown, PeriodicCountdownSaturatesAtZero) {
    std::array<std::int16_t, 3> counters{2, 5, 0};

    ml::tick_periodic_countdowns<std::int16_t>(counters, 3);

    EXPECT_EQ(counters, (std::array<std::int16_t, 3>{0, 2, 0}));
}

TEST(NativeCountdown, RestartsReadyCounters) {
    std::array<std::int32_t, 4> counters{-1, 0, 1, 2};

    ml::consume_ready_countdowns<std::int32_t>(counters, 10);

    EXPECT_EQ(counters, (std::array<std::int32_t, 4>{10, 10, 1, 2}));
}

TEST(NativeCountdown, DecrementsFloatingPointTimers) {
    std::array counters{2.0f, 1.0f};

    ml::tick_countdowns(counters, 0.25f);

    EXPECT_EQ(counters, (std::array{1.75f, 0.75f}));
}

TEST(NativeTickCountdown, OwnsAndMutatesNativeStorage) {
    ml::TickCountdown<std::int16_t> countdown{3, 5};

    countdown.set_counter(0, 2);
    countdown.set_counter(1, 1);
    countdown.zero_counter(2);
    countdown.tick();

    EXPECT_EQ(copy_values(countdown.counters()), (std::vector<std::int16_t>{1, 0, -1}));
    EXPECT_FALSE(countdown.try_consume(0));
    EXPECT_TRUE(countdown.try_consume(1));
    EXPECT_TRUE(countdown.try_consume(2));
    EXPECT_EQ(copy_values(countdown.counters()), (std::vector<std::int16_t>{1, 5, 5}));
}

TEST(NativeTickCountdown, CleanerAndResetSemanticsArePreserved) {
    ml::TickCountdown<std::int8_t> countdown{1, 0};

    for (int index{}; index < 32; ++index) {
        countdown.tick();
    }

    countdown.reset();
    countdown.add_zeroed(1);
    for (int index{}; index < 63; ++index) {
        countdown.tick();
    }
    EXPECT_EQ(countdown.counters()[0], -63);

    countdown.tick();
    EXPECT_EQ(countdown.counters()[0], 0);
}

TEST(NativeTickCountdown, SupportsSoaRemovalAndPermutation) {
    ml::TickCountdown<std::int16_t> countdown{3, 6};
    countdown.set_counter(0, 1);
    countdown.set_counter(1, 2);
    countdown.set_counter(2, 3);

    std::array<std::int32_t, 3> indices{2, 0, 1};
    countdown.apply_permutation(indices);
    EXPECT_EQ(copy_values(countdown.counters()), (std::vector<std::int16_t>{3, 1, 2}));
    EXPECT_EQ(indices, (std::array<std::int32_t, 3>{2, 0, 1}));

    countdown.remove_at_swap(0, 1);
    EXPECT_EQ(copy_values(countdown.counters()), (std::vector<std::int16_t>{2, 1}));
}

TEST(NativePeriodicTickCountdown, UsesIndividualPeriods) {
    ml::PeriodicTickCountdown<std::int8_t> countdown;
    countdown.add_started(2);
    countdown.add_started(4);

    countdown.tick();
    countdown.tick();
    EXPECT_TRUE(countdown.try_consume(0));
    EXPECT_FALSE(countdown.try_consume(1));
    EXPECT_EQ(copy_values(countdown.remaining_ticks()), (std::vector<std::int8_t>{2, 2}));
    EXPECT_EQ(copy_values(countdown.periods()), (std::vector<std::int8_t>{2, 4}));
}

TEST(NativePeriodicTickCountdown, PreservesPairsAcrossSoaOperations) {
    ml::PeriodicTickCountdown<std::int16_t> source;
    source.add_started(10);
    source.add_started(20);
    source.add_started(30);
    source.tick(3);

    ml::PeriodicTickCountdown<std::int16_t> destination;
    destination.add_zeroed(1, 3);
    destination.copy_element(0, source, 2);
    destination.copy_elements(1, source, 0, 2);

    EXPECT_EQ(copy_values(destination.remaining_ticks()), (std::vector<std::int16_t>{27, 7, 17}));
    EXPECT_EQ(copy_values(destination.periods()), (std::vector<std::int16_t>{30, 10, 20}));

    destination.remove_at_swap(1, 1);
    EXPECT_EQ(copy_values(destination.remaining_ticks()), (std::vector<std::int16_t>{27, 17}));
    EXPECT_EQ(copy_values(destination.periods()), (std::vector<std::int16_t>{30, 20}));
}

TEST(NativeCountdownTimers, TicksAndSupportsNativeStorageOperations) {
    ml::CountdownTimers timers;
    timers.add_defaulted(3);
    timers[0] = 1.f;
    timers[1] = 2.f;
    timers[2] = 3.f;

    timers.tick(0.5f);
    EXPECT_EQ(copy_values(timers.remaining_times()), (std::vector<float>{0.5f, 1.5f, 2.5f}));

    timers.remove_at_swap(0, 1);
    EXPECT_EQ(copy_values(timers.remaining_times()), (std::vector<float>{2.5f, 1.5f}));

    std::array<std::int32_t, 2> indices{1, 0};
    timers.apply_permutation(indices);
    EXPECT_EQ(copy_values(timers.remaining_times()), (std::vector<float>{1.5f, 2.5f}));
    EXPECT_EQ(indices, (std::array<std::int32_t, 2>{1, 0}));
}

TEST(NativeCountdownTimers, SortsUsingAndRestoresScratchIndices) {
    ml::CountdownTimers timers;
    static_cast<void>(timers.add(3.f));
    static_cast<void>(timers.add(1.f));
    static_cast<void>(timers.add(4.f));
    static_cast<void>(timers.add(2.f));
    std::array<std::int32_t, 4> scratch{};

    timers.sort([](ml::CountdownTimers const& values,
                   auto const lhs,
                   auto const rhs) { return values[lhs] < values[rhs]; },
                scratch);

    EXPECT_EQ(copy_values(timers.remaining_times()), (std::vector<float>{1.f, 2.f, 3.f, 4.f}));
    EXPECT_EQ(scratch, (std::array<std::int32_t, 4>{1, 3, 0, 2}));
}

TEST(NativePeriodicCountdownTimers, ConsumesAndPreservesPeriods) {
    ml::PeriodicCountdownTimers timers;
    timers.add_started(1.f);
    timers.add_started(0.5f);
    timers.add_zeroed(2.f);

    timers.tick(0.5f);

    EXPECT_FALSE(timers.expired(0));
    EXPECT_TRUE(timers.try_consume(1));
    EXPECT_TRUE(timers.try_consume(2));
    EXPECT_EQ(copy_values(timers.remaining_times()), (std::vector<float>{0.5f, 0.5f, 2.f}));
    EXPECT_EQ(copy_values(timers.periods()), (std::vector<float>{1.f, 0.5f, 2.f}));

    timers.tick(2.5f);
    EXPECT_TRUE(timers.try_consume(0));
    EXPECT_EQ(timers[0], 1.f);
}
