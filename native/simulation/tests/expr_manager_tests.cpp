#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/expressions/expr_manager.h>
#include <ioj/sim/expressions/timers.h>
#include <ioj/sim/missions/mission.h>
#include <ioj/sim/sim_clock.h>

#include <gtest/gtest.h>

#include <array>

namespace ioj::sim::expression_tests {
using namespace ioj::levels;

auto timer(double const seconds) -> MissionExpr {
    return {TimeElapsedExpr{.seconds = NonNegativeFinite<double>::make(seconds)}};
}

TEST(Expressions, SharedDagEvaluatesChildrenFirstWithoutRetiringSharedLeaves) {
    SimClock clock;
    clock.initialise({.tick_rate = Frequency::make(10.0)});
    EntityLedger ledger;
    ExprManager expressions;
    Timers timers;
    Mission mission;
    std::array const nodes{MissionExpr{AllExpr{.children = {1, 2}}},
                           MissionExpr{AllExpr{.children = {3, 3, 4}}},
                           MissionExpr{AnyExpr{.children = {3, 4}}},
                           timer(1),
                           timer(2)};
    auto const compiled{expressions.append(nodes, {{}, {}, clock, timers})};
    ASSERT_TRUE(compiled);
    auto const& handles{compiled->handles};
    expressions.activate(handles[0]);
    for (auto const handle : compiled->timers) {
        timers.start(handle, SimTick{});
    }
    EXPECT_EQ(expressions.leaves().active.size(), 2);

    expressions.evaluate({SimTick{10}, ledger, timers, {}}, mission);
    EXPECT_EQ(expressions.result(handles[2]), ExprResult::True);
    EXPECT_EQ(expressions.result(handles[4]), ExprResult::Pending);
    EXPECT_EQ(expressions.result(handles[0]), ExprResult::Pending);
    EXPECT_EQ(expressions.leaves().active.size(), 1);

    expressions.evaluate({SimTick{20}, ledger, timers, {}}, mission);
    EXPECT_EQ(expressions.result(handles[0]), ExprResult::True);
    EXPECT_TRUE(expressions.leaves().active.empty());
}

TEST(Expressions, MixedPayloadIndicesWaitForFutureEntityDestruction) {
    SimClock clock;
    clock.initialise({.tick_rate = Frequency::make(10.0)});
    EntityLedger ledger;
    ExprManager expressions;
    Timers timers;
    Mission mission;
    std::array<EntityId, 1> entities;
    std::array const nodes{timer(1),
                           MissionExpr{EntityDestroyedExpr{levels::EntityId{"future"}}},
                           timer(2),
                           MissionExpr{AllExpr{.children = {0, 1, 2}}}};
    auto const compiled{
        expressions.append(nodes, {{{levels::EntityId{"future"}, 0}}, {}, clock, timers})};
    ASSERT_TRUE(compiled);
    auto const& handles{compiled->handles};
    expressions.activate(handles[3]);
    for (auto const handle : compiled->timers) {
        timers.start(handle, SimTick{});
    }
    expressions.evaluate({SimTick{10}, ledger, timers, entities}, mission);
    EXPECT_EQ(expressions.result(handles[0]), ExprResult::True);
    EXPECT_EQ(expressions.result(handles[2]), ExprResult::Pending);
    expressions.evaluate({SimTick{20}, ledger, timers, entities}, mission);
    EXPECT_EQ(expressions.result(handles[2]), ExprResult::True);
    EXPECT_EQ(expressions.result(handles[3]), ExprResult::Pending);

    ledger.record_spawns(EntityType::Fighter, Team::Red, Health{1}, entities);
    expressions.evaluate({SimTick{21}, ledger, timers, entities}, mission);
    EXPECT_EQ(expressions.result(handles[1]), ExprResult::Pending);
    EntityDeathInfo deaths;
    deaths.add_defaulted(1);
    deaths.view().victims()[0] = entities[0];
    deaths.view().reasons()[0] = DeathReason::Combat;
    ledger.record_deaths(deaths.const_view(), SimTick{22});
    expressions.evaluate({SimTick{22}, ledger, timers, entities}, mission);
    EXPECT_EQ(expressions.result(handles[3]), ExprResult::True);
}

TEST(Expressions, ActivationDoesNotStartTimersAndStartIsVisibleOnNextEvaluation) {
    SimClock clock;
    clock.initialise({.tick_rate = Frequency::make(10.0)});
    EntityLedger ledger;
    ExprManager expressions;
    Timers timers;
    Mission mission;
    std::array const nodes{timer(0), timer(0.21)};
    auto const compiled{expressions.append(nodes, {{}, {}, clock, timers})};
    ASSERT_TRUE(compiled);
    ASSERT_EQ(compiled->timers.size(), 2);
    auto const zero{compiled->handles[0]};
    auto const delayed{compiled->handles[1]};
    expressions.activate(compiled->leaves, compiled->composites);
    expressions.evaluate({SimTick{100}, ledger, timers, {}}, mission);
    EXPECT_EQ(expressions.result(zero), ExprResult::Pending);
    EXPECT_EQ(expressions.result(delayed), ExprResult::Pending);

    for (auto const handle : compiled->timers) {
        timers.start(handle, SimTick{100});
    }
    EXPECT_EQ(expressions.result(zero), ExprResult::Pending);
    expressions.evaluate({SimTick{101}, ledger, timers, {}}, mission);
    EXPECT_EQ(expressions.result(zero), ExprResult::True);
    EXPECT_EQ(expressions.result(delayed), ExprResult::Pending);
    expressions.deactivate(delayed);
    expressions.activate(delayed);
    expressions.evaluate({SimTick{102}, ledger, timers, {}}, mission);
    EXPECT_EQ(expressions.result(delayed), ExprResult::Pending);
    expressions.evaluate({SimTick{103}, ledger, timers, {}}, mission);
    EXPECT_EQ(expressions.result(delayed), ExprResult::True);
}
}
