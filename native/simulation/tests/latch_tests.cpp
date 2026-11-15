#include <ioj/frame_memory_resource.h>
#include <ioj/levels/expression.h>
#include <ioj/sim/collision/spatial_query_manager.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/entities/entity_lookup_tables.h>
#include <ioj/sim/expressions/condition_manager.h>
#include <ioj/sim/expressions/expr_compilation_context.h>
#include <ioj/sim/expressions/expr_evaluation_context.h>
#include <ioj/sim/expressions/timers.h>

#include <gtest/gtest.h>

#include <array>
#include <initializer_list>
#include <utility>
#include <vector>

namespace ioj::sim::latch_tests {
struct Samples {
    auto compile(std::initializer_list<levels::Expression> nodes) -> std::vector<ExprHandle> {
        std::vector<ExprHandle> handles(nodes.size());
        conditions.append(nodes, {{}, std::span{&timer, 1}}, handles);
        return handles;
    }
    void evaluate() { conditions.evaluate({ledger, timers, {}, spatial, lookups, scratch}); }

    ConditionManager conditions;
    Timers timers;
    TimerHandle timer{timers.add()};
    EntityLedger ledger;
    EntityLookupTables lookups;
    SpatialQueryManager spatial;
    FrameMemoryResource scratch{4096};
};

TEST(Latches, UnsetUntilEnabledAndCommitted) {
    Samples samples;
    auto const handles{samples.compile({{levels::TimerElapsedExpr{0}}, {levels::AllExpr{}}})};
    auto& conditions{samples.conditions};
    auto const latch{conditions.add_latch({handles[0], handles[1]})};
    samples.evaluate();
    conditions.commit_latches();
    EXPECT_EQ(conditions.latch_state(latch), LatchState::Unset);

    samples.timers.start(samples.timer, 1);
    samples.timers.tick(1);
    samples.evaluate();
    EXPECT_EQ(conditions.latch_state(latch), LatchState::Unset);
    conditions.commit_latches();
    EXPECT_EQ(conditions.latch_state(latch), LatchState::True);
}

TEST(Latches, CapturesBothValuesOnlyOnceIncludingBeforeCommit) {
    for (bool const value : {false, true}) {
        Samples samples;
        auto const handles{samples.compile(
            {{levels::AllExpr{}}, {levels::TimerElapsedExpr{0}}, {levels::NotExpr{{1}}}})};
        auto& conditions{samples.conditions};
        auto const latch{conditions.add_latch({handles[0], handles[value ? 2 : 1]})};
        samples.evaluate();
        samples.timers.start(samples.timer, 1);
        samples.timers.tick(1);
        samples.evaluate();
        conditions.commit_latches();
        EXPECT_EQ(conditions.latch_state(latch), value ? LatchState::True : LatchState::False);
        samples.evaluate();
        conditions.commit_latches();
        EXPECT_EQ(conditions.latch_state(latch), value ? LatchState::True : LatchState::False);
    }
}

TEST(Latches, ChainedCapturesReadPreviousCycleRegardlessOfEvaluationOrder) {
    for (bool const reverse : {false, true}) {
        Samples samples;
        auto const handles{
            samples.compile({{levels::AllExpr{}}, {levels::LatchedTrueExpr{reverse ? 1u : 0u}}})};
        auto& conditions{samples.conditions};
        LatchInput const first_input{handles[0], handles[0]};
        LatchInput const second_input{handles[1], handles[0]};
        auto first{conditions.add_latch(reverse ? second_input : first_input)};
        auto second{conditions.add_latch(reverse ? first_input : second_input)};
        if (reverse) {
            std::swap(first, second);
        }
        samples.evaluate();
        EXPECT_EQ(conditions.latch_state(first), LatchState::Unset);
        EXPECT_EQ(conditions.latch_state(second), LatchState::Unset);
        conditions.commit_latches();
        EXPECT_EQ(conditions.latch_state(first), LatchState::True);
        EXPECT_EQ(conditions.latch_state(second), LatchState::Unset);
        samples.evaluate();
        EXPECT_EQ(conditions.latch_state(second), LatchState::Unset);
        conditions.commit_latches();
        EXPECT_EQ(conditions.latch_state(second), LatchState::True);
    }
}

TEST(Latches, CapturedValueAlsoReadsOnlyCommittedState) {
    Samples samples;
    auto const handles{samples.compile({{levels::AllExpr{}}, {levels::LatchedTrueExpr{0}}})};
    auto& conditions{samples.conditions};
    auto const first{conditions.add_latch({handles[0], handles[0]})};
    auto const second{conditions.add_latch({handles[0], handles[1]})};
    samples.evaluate();
    EXPECT_EQ(conditions.latch_state(first), LatchState::Unset);
    EXPECT_EQ(conditions.latch_state(second), LatchState::Unset);
    conditions.commit_latches();
    EXPECT_EQ(conditions.latch_state(first), LatchState::True);
    EXPECT_EQ(conditions.latch_state(second), LatchState::False);
}

TEST(Latches, RetirementPreservesUncapturedEntriesAndStableHandles) {
    Samples samples;
    auto const handles{
        samples.compile({{levels::AllExpr{}}, {levels::AnyExpr{}}, {levels::TimerElapsedExpr{0}}})};
    auto& conditions{samples.conditions};
    std::array const latches{
        conditions.add_latch({handles[0], handles[1]}),
        conditions.add_latch({handles[2], handles[0]}),
        conditions.add_latch({handles[0], handles[0]}),
        conditions.add_latch({handles[2], handles[0]}),
    };
    samples.evaluate();
    conditions.commit_latches();
    EXPECT_EQ(conditions.latch_state(latches[0]), LatchState::False);
    EXPECT_EQ(conditions.latch_state(latches[2]), LatchState::True);
    for (auto const index : {1, 3}) {
        EXPECT_EQ(conditions.latch_state(latches[index]), LatchState::Unset);
    }
    samples.timers.start(samples.timer, 1);
    samples.timers.tick(1);
    samples.evaluate();
    conditions.commit_latches();
    EXPECT_EQ(conditions.latch_state(latches[0]), LatchState::False);
    for (auto const index : {1, 2, 3}) {
        EXPECT_EQ(conditions.latch_state(latches[index]), LatchState::True);
    }
}

TEST(Latches, SeparateInstancesStartUnsetAndDoNotShareCaptures) {
    Samples first;
    Samples second;
    auto const first_handles{first.compile({{levels::AllExpr{}}})};
    auto const second_handles{second.compile({{levels::AllExpr{}}, {levels::AnyExpr{}}})};
    auto const a{first.conditions.add_latch({first_handles[0], first_handles[0]})};
    auto const b{second.conditions.add_latch({second_handles[0], second_handles[1]})};
    first.evaluate();
    second.conditions.commit_latches();
    EXPECT_EQ(second.conditions.latch_state(b), LatchState::Unset);
    first.conditions.commit_latches();
    second.evaluate();
    second.conditions.commit_latches();
    EXPECT_EQ(first.conditions.latch_state(a), LatchState::True);
    EXPECT_EQ(second.conditions.latch_state(b), LatchState::False);
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
