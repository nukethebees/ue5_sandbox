#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/expressions/expr_manager.h>
#include <ioj/sim/expressions/latches.h>
#include <ioj/sim/missions/mission.h>
#include <ioj/sim/sim_clock.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

namespace ioj::sim::latch_tests {
struct ExpressionSamples {
    ExprManager expressions;
    std::vector<ExprHandle> handles;
};

auto sample(std::initializer_list<bool> const values) -> ExpressionSamples {
    std::vector<levels::MissionExpr> nodes{{levels::AllExpr{}}, {levels::AnyExpr{}}};
    for (bool const value : values) {
        nodes.emplace_back(levels::AllExpr{.children = {value ? 0u : 1u}});
    }

    SimClock clock;
    EntityLedger ledger;
    Mission mission;
    ExpressionSamples result;
    auto const compiled{result.expressions.append(nodes, {{}, {}, clock}).value()};
    result.handles.assign(compiled.handles.begin() + 2, compiled.handles.end());
    result.expressions.activate(compiled.leaves, compiled.composites, SimTick{});
    result.expressions.evaluate({SimTick{}, ledger, {}}, mission);
    return result;
}

TEST(Latches, UnsetUntilEnabledAndCommitted) {
    auto samples{sample({false, true})};
    LatchInput const input{samples.handles[0], samples.handles[1]};
    Latches latches;
    auto const handle{latches.add(input)};
    EXPECT_FALSE(latches.latched_true(handle));
    EXPECT_FALSE(latches.latched_false(handle));

    latches.evaluate(samples.expressions);
    latches.commit();
    EXPECT_FALSE(latches.latched_true(handle));
    EXPECT_FALSE(latches.latched_false(handle));

    samples = sample({true, true});
    latches.evaluate(samples.expressions);
    EXPECT_FALSE(latches.latched_true(handle));
    EXPECT_FALSE(latches.latched_false(handle));

    latches.commit();
    EXPECT_TRUE(latches.latched_true(handle));
    EXPECT_FALSE(latches.latched_false(handle));
}

TEST(Latches, CapturesBothValuesOnlyOnceIncludingBeforeCommit) {
    for (bool const value : {false, true}) {
        auto samples{sample({true, value})};
        Latches latches;
        auto const handle{latches.add({samples.handles[0], samples.handles[1]})};
        latches.evaluate(samples.expressions);

        samples = sample({true, !value});
        latches.evaluate(samples.expressions);
        latches.commit();
        EXPECT_EQ(latches.latched_true(handle), value);
        EXPECT_EQ(latches.latched_false(handle), !value);

        latches.evaluate(samples.expressions);
        latches.commit();
        EXPECT_EQ(latches.latched_true(handle), value);
        EXPECT_EQ(latches.latched_false(handle), !value);
    }
}

TEST(Latches, ChainedCapturesReadPreviousCycleRegardlessOfEvaluationOrder) {
    for (bool const reverse : {false, true}) {
        auto samples{sample({true, false})};
        LatchInput const first_input{samples.handles[0], samples.handles[0]};
        LatchInput const second_input{samples.handles[1], samples.handles[0]};
        Latches latches;
        auto first{latches.add(reverse ? second_input : first_input)};
        auto second{latches.add(reverse ? first_input : second_input)};
        if (reverse) {
            std::swap(first, second);
        }

        latches.commit();
        samples = sample({true, latches.latched_true(first)});
        latches.evaluate(samples.expressions);
        EXPECT_FALSE(latches.latched_true(first));
        EXPECT_FALSE(latches.latched_true(second));

        latches.commit();
        EXPECT_TRUE(latches.latched_true(first));
        EXPECT_FALSE(latches.latched_true(second));
        samples = sample({true, latches.latched_true(first)});
        latches.evaluate(samples.expressions);
        EXPECT_FALSE(latches.latched_true(second));

        latches.commit();
        EXPECT_TRUE(latches.latched_true(second));
    }
}

TEST(Latches, CapturedValueAlsoReadsOnlyCommittedState) {
    auto samples{sample({true, false})};
    Latches latches;
    auto const first{latches.add({samples.handles[0], samples.handles[0]})};
    auto const second{latches.add({samples.handles[0], samples.handles[1]})};
    samples = sample({true, latches.latched_true(first)});
    latches.evaluate(samples.expressions);
    EXPECT_FALSE(latches.latched_true(first));
    EXPECT_FALSE(latches.latched_false(second));
    latches.commit();

    EXPECT_TRUE(latches.latched_true(first));
    EXPECT_TRUE(latches.latched_false(second));
}

TEST(Latches, RetirementPreservesUncapturedEntriesAndStableHandles) {
    auto samples{sample({true, false, true, false})};
    Latches latches;
    std::array const handles{
        latches.add({samples.handles[0], samples.handles[1]}),
        latches.add({samples.handles[1], samples.handles[0]}),
        latches.add({samples.handles[2], samples.handles[0]}),
        latches.add({samples.handles[3], samples.handles[0]}),
    };

    latches.evaluate(samples.expressions);
    latches.commit();
    EXPECT_FALSE(latches.latched_true(handles[1]));
    EXPECT_FALSE(latches.latched_false(handles[1]));
    EXPECT_FALSE(latches.latched_true(handles[3]));
    EXPECT_FALSE(latches.latched_false(handles[3]));
    EXPECT_TRUE(latches.latched_false(handles[0]));
    EXPECT_TRUE(latches.latched_true(handles[2]));

    samples = sample({true, true, true, true});
    latches.evaluate(samples.expressions);
    latches.commit();
    EXPECT_TRUE(latches.latched_false(handles[0]));
    EXPECT_TRUE(latches.latched_true(handles[1]));
    EXPECT_TRUE(latches.latched_true(handles[2]));
    EXPECT_TRUE(latches.latched_true(handles[3]));
}

TEST(Latches, SeparateInstancesStartUnsetAndDoNotShareCaptures) {
    auto const samples{sample({true, false})};
    Latches first;
    auto const first_handle{first.add({samples.handles[0], samples.handles[0]})};
    Latches second;
    auto const second_handle{second.add({samples.handles[0], samples.handles[1]})};
    first.evaluate(samples.expressions);
    second.commit();
    EXPECT_FALSE(second.latched_true(second_handle));
    EXPECT_FALSE(second.latched_false(second_handle));

    first.commit();
    second.evaluate(samples.expressions);
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
