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
#include <unordered_map>

namespace ioj::sim::expression_tests {
using namespace ioj::levels;

TEST(Expressions, BooleanOperatorsEvaluateSharedLeavesInDependencyOrder) {
    EntityLedger ledger;
    EntityLookupTables lookups{};
    SpatialQueryManager spatial{};
    FrameMemoryResource scratch{4096};
    std::array const nodes{
        Expression{AllExpr{{1, 2}}},
        Expression{NotExpr{{3}}},
        Expression{XorExpr{{3, 4, 4}}},
        Expression{TimerElapsedExpr{0}},
        Expression{TimerElapsedExpr{1}},
        Expression{AnyExpr{{3, 4}}},
        Expression{AllExpr{}},
        Expression{AnyExpr{}},
        Expression{XorExpr{}},
    };
    std::span<EntityId const> const no_entities{};
    for (auto const a : {false, true}) {
        for (auto const b : {false, true}) {
            Timers timers;
            std::array const timer_handles{timers.add(), timers.add()};
            if (a) {
                timers.start(timer_handles[0], 0);
            }
            if (b) {
                timers.start(timer_handles[1], 0);
            }
            timers.tick(1);

            ConditionManager expressions;
            std::array<ExprHandle, nodes.size()> handles;
            expressions.append(nodes, {{}, timer_handles}, handles);
            expressions.evaluate({ledger, timers, no_entities, spatial, lookups, scratch});
            EXPECT_FALSE(expressions.result(handles[0]));
            EXPECT_EQ(expressions.result(handles[1]), !a);
            EXPECT_EQ(expressions.result(handles[2]), a);
            EXPECT_EQ(expressions.result(handles[5]), a || b);
            EXPECT_TRUE(expressions.result(handles[6]));
            EXPECT_FALSE(expressions.result(handles[7]));
            EXPECT_FALSE(expressions.result(handles[8]));
        }
    }
}

TEST(Expressions, LatchObservationsAndCompositesUseTheSameCycleSample) {
    EntityLedger ledger;
    EntityLookupTables lookups{};
    SpatialQueryManager spatial{};
    FrameMemoryResource scratch{4096};
    Timers timers;
    ConditionManager expressions;
    std::array const nodes{
        Expression{AllExpr{}},
        Expression{LatchedTrueExpr{0}},
        Expression{AllExpr{{1}}},
        Expression{LatchedTrueExpr{1}},
        Expression{LatchedFalseExpr{0}},
    };
    std::array<ExprHandle, nodes.size()> handles;
    expressions.append(nodes, {{}, {}}, handles);

    expressions.add_latch({handles[0], handles[0]});
    expressions.add_latch({handles[2], handles[0]});
    ExprEvaluationContext const inputs{ledger, timers, {}, spatial, lookups, scratch};
    for (int cycle{}; cycle < 3; ++cycle) {
        expressions.commit_latches();
        expressions.evaluate(inputs);
        EXPECT_EQ(expressions.result(handles[1]), cycle >= 1);
        EXPECT_EQ(expressions.result(handles[2]), cycle >= 1);
        EXPECT_EQ(expressions.result(handles[3]), cycle >= 2);
        EXPECT_FALSE(expressions.result(handles[4]));
    }
}

auto timer(std::uint32_t index) -> Expression {
    return {TimerElapsedExpr{index}};
}

TEST(Expressions, SharedDagEvaluatesChildrenFirstWithoutRetiringSharedLeaves) {
    EntityLedger ledger;
    EntityLookupTables lookups{};
    SpatialQueryManager spatial{};
    FrameMemoryResource scratch{4096};
    ConditionManager expressions;
    Timers timers;
    auto const first{timers.add()};
    auto const second{timers.add()};
    std::array const nodes{Expression{AllExpr{.children = {1, 2}}},
                           Expression{AllExpr{.children = {3, 3, 4}}},
                           Expression{AnyExpr{.children = {3, 4}}},
                           timer(0),
                           timer(1)};
    std::array const timer_handles{first, second};
    std::array<ExprHandle, nodes.size()> handles;
    expressions.append(nodes, {{}, timer_handles}, handles);

    timers.start(first, 10);
    timers.start(second, 20);

    timers.tick(10);
    expressions.evaluate({ledger, timers, {}, spatial, lookups, scratch});
    EXPECT_EQ(expressions.result(handles[2]), true);
    EXPECT_EQ(expressions.result(handles[4]), false);
    EXPECT_EQ(expressions.result(handles[0]), false);

    timers.tick(10);
    expressions.evaluate({ledger, timers, {}, spatial, lookups, scratch});
    EXPECT_EQ(expressions.result(handles[0]), true);
}

TEST(Expressions, MixedPayloadIndicesWaitForFutureEntityDestruction) {
    EntityLedger ledger;
    EntityLookupTables lookups{};
    SpatialQueryManager spatial{};
    FrameMemoryResource scratch{4096};
    ConditionManager expressions;
    Timers timers;
    auto const first{timers.add()};
    auto const second{timers.add()};
    std::array<EntityId, 1> entities;
    std::array const nodes{timer(0),
                           Expression{EntityDestroyedExpr{levels::EntityId{"future"}}},
                           timer(1),
                           Expression{AllExpr{.children = {0, 1, 2}}}};
    std::array const timer_handles{first, second};
    std::unordered_map<levels::EntityId, LevelEntityRef> entity_refs;
    entity_refs.emplace(levels::EntityId{"future"}, LevelEntityRef{0});
    std::array<ExprHandle, nodes.size()> handles;
    expressions.append(nodes, {entity_refs, timer_handles}, handles);

    timers.start(first, 10);
    timers.start(second, 20);
    timers.tick(10);
    expressions.evaluate({ledger, timers, entities, spatial, lookups, scratch});
    EXPECT_EQ(expressions.result(handles[0]), true);
    EXPECT_EQ(expressions.result(handles[2]), false);
    timers.tick(10);
    expressions.evaluate({ledger, timers, entities, spatial, lookups, scratch});
    EXPECT_EQ(expressions.result(handles[2]), true);
    EXPECT_EQ(expressions.result(handles[3]), false);

    ledger.record_spawns(EntityType::Fighter, Team::Red, Health{1}, entities);
    timers.tick(1);
    expressions.evaluate({ledger, timers, entities, spatial, lookups, scratch});
    EXPECT_EQ(expressions.result(handles[1]), false);
    EntityDeathInfo deaths;
    deaths.add_defaulted(1);
    deaths.view().victims()[0] = entities[0];
    deaths.view().reasons()[0] = DeathReason::Combat;
    ledger.record_deaths(deaths.const_view(), SimTick{22});
    timers.tick(1);
    expressions.evaluate({ledger, timers, entities, spatial, lookups, scratch});
    EXPECT_EQ(expressions.result(handles[3]), true);
}

TEST(Expressions, StoppedTimersAndStartAreVisibleOnNextEvaluation) {
    EntityLedger ledger;
    EntityLookupTables lookups{};
    SpatialQueryManager spatial{};
    FrameMemoryResource scratch{4096};
    ConditionManager expressions;
    Timers timers;
    auto const first{timers.add()};
    auto const second{timers.add()};
    auto const zero{expressions.add_timer_elapsed(first)};
    auto const delayed{expressions.add_timer_elapsed(second)};
    timers.tick(100);
    expressions.evaluate({ledger, timers, {}, spatial, lookups, scratch});
    EXPECT_EQ(expressions.result(zero), false);
    EXPECT_EQ(expressions.result(delayed), false);

    timers.start(first, 0);
    timers.start(second, 3);
    EXPECT_EQ(expressions.result(zero), false);
    timers.tick(1);
    EXPECT_EQ(expressions.result(zero), false);
    expressions.evaluate({ledger, timers, {}, spatial, lookups, scratch});
    EXPECT_EQ(expressions.result(zero), true);
    EXPECT_EQ(expressions.result(delayed), false);
    timers.tick(1);
    expressions.evaluate({ledger, timers, {}, spatial, lookups, scratch});
    EXPECT_EQ(expressions.result(delayed), false);
    timers.tick(1);
    expressions.evaluate({ledger, timers, {}, spatial, lookups, scratch});
    EXPECT_EQ(expressions.result(delayed), true);
}

TEST(Expressions, DynamicRegistrationPreservesExistingHandlesAndLatchSampling) {
    EntityLedger ledger;
    EntityLookupTables lookups{};
    SpatialQueryManager spatial{};
    FrameMemoryResource scratch{4096};
    Timers timers;
    ConditionManager expressions;
    auto const first{timers.add()};
    auto const initial{expressions.add_timer_elapsed(first)};
    timers.start(first, 0);
    timers.tick(1);
    expressions.evaluate({ledger, timers, {}, spatial, lookups, scratch});
    ASSERT_TRUE(expressions.result(initial));

    auto const latch{expressions.add_latch({initial, initial})};
    auto const second{timers.add()};
    auto const dynamic{expressions.add_timer_elapsed(second)};
    std::array const timer_handles{second};
    std::array const nodes{
        Expression{AllExpr{{1, 2}}},
        Expression{LatchedTrueExpr{latch.index()}},
        Expression{TimerElapsedExpr{0}},
        Expression{NotExpr{{2}}},
        Expression{LatchedFalseExpr{latch.index()}},
    };
    std::array<ExprHandle, nodes.size()> handles;
    expressions.append(nodes, {{}, timer_handles}, handles);
    EXPECT_EQ(initial.kind(), ExprKind::TimerElapsed);
    EXPECT_EQ(handles[0].kind(), ExprKind::All);
    EXPECT_EQ(handles[1].kind(), ExprKind::LatchedTrue);
    EXPECT_NE(initial.index(), dynamic.index());
    EXPECT_NE(dynamic.index(), handles[2].index());

    expressions.evaluate({ledger, timers, {}, spatial, lookups, scratch});
    EXPECT_TRUE(expressions.result(initial));
    EXPECT_TRUE(expressions.result(handles[3]));
    EXPECT_FALSE(expressions.result(handles[1]));
    expressions.commit_latches();
    timers.start(second, 0);
    timers.tick(1);
    expressions.evaluate({ledger, timers, {}, spatial, lookups, scratch});
    EXPECT_TRUE(expressions.result(initial));
    EXPECT_TRUE(expressions.result(dynamic));
    EXPECT_TRUE(expressions.result(handles[0]));
    EXPECT_TRUE(expressions.result(handles[1]));
    EXPECT_FALSE(expressions.result(handles[3]));
    EXPECT_FALSE(expressions.result(handles[4]));
}
}
