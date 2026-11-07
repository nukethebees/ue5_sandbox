#include <ioj/sim/expressions/latches.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>

namespace ioj::sim::latch_tests {
inline constexpr auto when_expression{ExprHandle::make_data_composite(CompositeKind::All, 0)};
inline constexpr auto value_expression{ExprHandle::make_data_composite(CompositeKind::Any, 1)};

TEST(Latches, UnsetUntilEnabledAndCommitted) {
    Latches latches;
    auto const handle{latches.add(when_expression, value_expression)};
    EXPECT_EQ(latches.inputs(handle).when, when_expression);
    EXPECT_EQ(latches.inputs(handle).value, value_expression);
    EXPECT_EQ(latches.state(handle), LatchState::Unset);
    EXPECT_FALSE(latches.latched_true(handle));
    EXPECT_FALSE(latches.latched_false(handle));

    latches.evaluate(handle, false, true);
    latches.commit();
    EXPECT_EQ(latches.state(handle), LatchState::Unset);
    EXPECT_EQ(latches.active().size(), 1);

    latches.evaluate(handle, true, true);
    EXPECT_FALSE(latches.latched_true(handle));
    EXPECT_FALSE(latches.latched_false(handle));
    EXPECT_EQ(latches.active().size(), 1);

    latches.commit();
    EXPECT_TRUE(latches.latched_true(handle));
    EXPECT_FALSE(latches.latched_false(handle));
    EXPECT_TRUE(latches.active().empty());
}

TEST(Latches, CapturesBothValuesOnlyOnceIncludingBeforeCommit) {
    for (bool const value : {false, true}) {
        Latches latches;
        auto const handle{latches.add(when_expression, value_expression)};
        latches.evaluate(handle, true, value);
        latches.evaluate(handle, true, !value);
        latches.commit();
        EXPECT_EQ(latches.latched_true(handle), value);
        EXPECT_EQ(latches.latched_false(handle), !value);

        latches.evaluate(handle, true, !value);
        latches.commit();
        EXPECT_EQ(latches.latched_true(handle), value);
        EXPECT_EQ(latches.latched_false(handle), !value);
    }
}

TEST(Latches, ChainedCapturesReadPreviousCycleRegardlessOfEvaluationOrder) {
    for (bool const reverse : {false, true}) {
        Latches latches;
        auto const first{latches.add(when_expression, value_expression)};
        auto const second{latches.add(when_expression, value_expression)};
        auto order{std::array{first, second}};
        if (reverse) {
            std::ranges::reverse(order);
        }

        latches.commit();
        for (auto const handle : order) {
            latches.evaluate(handle, handle == first || latches.latched_true(first), true);
        }
        EXPECT_FALSE(latches.latched_true(first));
        EXPECT_FALSE(latches.latched_true(second));

        latches.commit();
        EXPECT_TRUE(latches.latched_true(first));
        EXPECT_FALSE(latches.latched_true(second));
        for (auto const handle : latches.active()) {
            latches.evaluate(handle, latches.latched_true(first), true);
        }
        EXPECT_FALSE(latches.latched_true(second));

        latches.commit();
        EXPECT_TRUE(latches.latched_true(second));
        EXPECT_TRUE(latches.active().empty());
    }
}

TEST(Latches, CapturedValueAlsoReadsOnlyCommittedState) {
    Latches latches;
    auto const first{latches.add(when_expression, value_expression)};
    auto const second{latches.add(when_expression, value_expression)};
    latches.evaluate(first, true, true);
    latches.evaluate(second, true, latches.latched_true(first));
    latches.commit();

    EXPECT_TRUE(latches.latched_true(first));
    EXPECT_TRUE(latches.latched_false(second));
}

TEST(Latches, RetirementPreservesUncapturedEntriesAndStableHandles) {
    Latches latches;
    std::array<LatchHandle, 4> handles;
    for (auto& handle : handles) {
        handle = latches.add(when_expression, value_expression);
    }

    latches.evaluate(handles[0], true, false);
    latches.evaluate(handles[2], true, true);
    latches.commit();
    ASSERT_EQ(latches.active().size(), 2);
    EXPECT_TRUE(std::ranges::contains(latches.active(), handles[1]));
    EXPECT_TRUE(std::ranges::contains(latches.active(), handles[3]));
    EXPECT_TRUE(latches.latched_false(handles[0]));
    EXPECT_TRUE(latches.latched_true(handles[2]));

    for (auto const handle : latches.active()) {
        latches.evaluate(handle, true, true);
    }
    latches.commit();
    EXPECT_TRUE(latches.active().empty());
    EXPECT_TRUE(latches.latched_false(handles[0]));
    EXPECT_TRUE(latches.latched_true(handles[1]));
    EXPECT_TRUE(latches.latched_true(handles[2]));
    EXPECT_TRUE(latches.latched_true(handles[3]));
}

TEST(Latches, SeparateInstancesStartUnsetAndDoNotShareCaptures) {
    Latches first;
    auto const first_handle{first.add(when_expression, value_expression)};
    Latches second;
    auto const second_handle{second.add(when_expression, value_expression)};
    first.evaluate(first_handle, true, true);
    second.commit();
    EXPECT_EQ(second.state(second_handle), LatchState::Unset);

    first.commit();
    second.evaluate(second_handle, true, false);
    second.commit();
    EXPECT_TRUE(first.latched_true(first_handle));
    EXPECT_TRUE(second.latched_false(second_handle));
}

TEST(Latches, PackedMemoryKeepsCachedAndPendingFieldsIndependent) {
    static_assert(sizeof(LatchMemory) == 1);
    static_assert(sizeof(LatchHandle) == sizeof(std::uint32_t));
    for (auto const value : {LatchState::False, LatchState::True}) {
        LatchMemory memory;
        EXPECT_EQ(memory.cached(), LatchState::Unset);
        EXPECT_EQ(memory.pending(), LatchState::Unset);
        memory.set_pending(value);
        EXPECT_EQ(memory.cached(), LatchState::Unset);
        EXPECT_EQ(memory.pending(), value);
        memory.set_cached(memory.pending());
        EXPECT_EQ(memory.cached(), value);
        EXPECT_EQ(memory.pending(), value);
    }
}
}
