#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/expressions/expr_manager.h>
#include <ioj/sim/expressions/latches.h>
#include <ioj/sim/expressions/timers.h>

#include <gtest/gtest.h>

#include <array>
#include <unordered_map>

namespace ioj::sim::expression_tests {
using namespace ioj::levels;

TEST(Expressions, BooleanOperatorsResampleSharedSignalsInDependencyOrder) {
    EntityLedger ledger;
    Timers timers;
    Latches latches;
    ExprManager expressions;
    std::array const nodes{
        Expression{AllExpr{{1, 2}}},
        Expression{NotExpr{{3}}},
        Expression{XorExpr{{3, 4, 4}}},
        Expression{EventFiredExpr{"a"}},
        Expression{EventFiredExpr{"b"}},
        Expression{AnyExpr{{3, 4}}},
        Expression{AllExpr{}},
        Expression{AnyExpr{}},
        Expression{XorExpr{}},
    };
    auto const handles{
        expressions.append(nodes, {{}, {}, {{"a", SignalHandle{0}}, {"b", SignalHandle{1}}}})};
    std::array<std::uint8_t, 2> signals{};
    for (auto const a : {false, true, false}) {
        for (auto const b : {false, true}) {
            signals = {static_cast<std::uint8_t>(a), static_cast<std::uint8_t>(b)};
            expressions.evaluate({ledger, timers, {}, latches, signals});
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
    Timers timers;
    Latches latches;
    ExprManager expressions;
    std::array const nodes{
        Expression{AllExpr{}},
        Expression{LatchedTrueExpr{0}},
        Expression{AllExpr{{1}}},
        Expression{LatchedTrueExpr{1}},
        Expression{LatchedFalseExpr{0}},
    };
    auto const handles{expressions.append(nodes, {{}, {}, {}})};
    latches.add({handles[0], handles[0]});
    latches.add({handles[2], handles[0]});
    for (int cycle{}; cycle < 3; ++cycle) {
        latches.commit();
        expressions.evaluate({ledger, timers, {}, latches, {}});
        latches.evaluate(expressions);
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
    ExprManager expressions;
    Timers timers;
    auto const first{timers.add()};
    auto const second{timers.add()};
    Latches latches;
    std::array const nodes{Expression{AllExpr{.children = {1, 2}}},
                           Expression{AllExpr{.children = {3, 3, 4}}},
                           Expression{AnyExpr{.children = {3, 4}}},
                           timer(0),
                           timer(1)};
    std::array const timer_handles{first, second};
    auto const compiled{expressions.append(nodes, {{}, timer_handles, {}})};
    auto const& handles{compiled};
    timers.start(first, 10);
    timers.start(second, 20);

    timers.tick(10);
    expressions.evaluate({ledger, timers, {}, latches, {}});
    EXPECT_EQ(expressions.result(handles[2]), true);
    EXPECT_EQ(expressions.result(handles[4]), false);
    EXPECT_EQ(expressions.result(handles[0]), false);

    timers.tick(10);
    expressions.evaluate({ledger, timers, {}, latches, {}});
    EXPECT_EQ(expressions.result(handles[0]), true);
}

TEST(Expressions, MixedPayloadIndicesWaitForFutureEntityDestruction) {
    EntityLedger ledger;
    ExprManager expressions;
    Timers timers;
    auto const first{timers.add()};
    auto const second{timers.add()};
    Latches latches;
    std::array<EntityId, 1> entities;
    std::array const nodes{timer(0),
                           Expression{EntityDestroyedExpr{levels::EntityId{"future"}}},
                           timer(1),
                           Expression{AllExpr{.children = {0, 1, 2}}}};
    std::array const timer_handles{first, second};
    std::unordered_map<levels::EntityId, LevelEntityRef> entity_refs;
    entity_refs.emplace(levels::EntityId{"future"}, LevelEntityRef{0});
    auto const compiled{expressions.append(nodes, {entity_refs, timer_handles, {}})};
    auto const& handles{compiled};
    timers.start(first, 10);
    timers.start(second, 20);
    timers.tick(10);
    expressions.evaluate({ledger, timers, entities, latches, {}});
    EXPECT_EQ(expressions.result(handles[0]), true);
    EXPECT_EQ(expressions.result(handles[2]), false);
    timers.tick(10);
    expressions.evaluate({ledger, timers, entities, latches, {}});
    EXPECT_EQ(expressions.result(handles[2]), true);
    EXPECT_EQ(expressions.result(handles[3]), false);

    ledger.record_spawns(EntityType::Fighter, Team::Red, Health{1}, entities);
    timers.tick(1);
    expressions.evaluate({ledger, timers, entities, latches, {}});
    EXPECT_EQ(expressions.result(handles[1]), false);
    EntityDeathInfo deaths;
    deaths.add_defaulted(1);
    deaths.view().victims()[0] = entities[0];
    deaths.view().reasons()[0] = DeathReason::Combat;
    ledger.record_deaths(deaths.const_view(), SimTick{22});
    timers.tick(1);
    expressions.evaluate({ledger, timers, entities, latches, {}});
    EXPECT_EQ(expressions.result(handles[3]), true);
}

TEST(Expressions, StoppedTimersAndStartAreVisibleOnNextEvaluation) {
    EntityLedger ledger;
    ExprManager expressions;
    Timers timers;
    Latches latches;
    auto const first{timers.add()};
    auto const second{timers.add()};
    auto const zero{expressions.add_timer_elapsed(first)};
    auto const delayed{expressions.add_timer_elapsed(second)};
    timers.tick(100);
    expressions.evaluate({ledger, timers, {}, latches, {}});
    EXPECT_EQ(expressions.result(zero), false);
    EXPECT_EQ(expressions.result(delayed), false);

    timers.start(first, 0);
    timers.start(second, 3);
    EXPECT_EQ(expressions.result(zero), false);
    timers.tick(1);
    EXPECT_EQ(expressions.result(zero), false);
    expressions.evaluate({ledger, timers, {}, latches, {}});
    EXPECT_EQ(expressions.result(zero), true);
    EXPECT_EQ(expressions.result(delayed), false);
    timers.tick(1);
    expressions.evaluate({ledger, timers, {}, latches, {}});
    EXPECT_EQ(expressions.result(delayed), false);
    timers.tick(1);
    expressions.evaluate({ledger, timers, {}, latches, {}});
    EXPECT_EQ(expressions.result(delayed), true);
}
}
