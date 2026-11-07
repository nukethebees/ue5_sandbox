#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/expressions/expr_manager.h>
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
    Mission mission;
    std::array const nodes{MissionExpr{AllExpr{.children = {1, 2}}},
                           MissionExpr{AllExpr{.children = {3, 3, 4}}},
                           MissionExpr{AnyExpr{.children = {3, 4}}},
                           timer(1),
                           timer(2)};
    auto const compiled{expressions.append(nodes, {{}, {}, clock})};
    ASSERT_TRUE(compiled);
    auto const& handles{compiled->handles};
    expressions.activate(handles[0], SimTick{0});
    EXPECT_EQ(expressions.leaves().active.size(), 2);

    expressions.evaluate({SimTick{10}, ledger, {}}, mission);
    EXPECT_EQ(expressions.result(handles[2]), ExprResult::True);
    EXPECT_EQ(expressions.result(handles[4]), ExprResult::Pending);
    EXPECT_EQ(expressions.result(handles[0]), ExprResult::Pending);
    EXPECT_EQ(expressions.leaves().active.size(), 1);

    expressions.evaluate({SimTick{20}, ledger, {}}, mission);
    EXPECT_EQ(expressions.result(handles[0]), ExprResult::True);
    EXPECT_TRUE(expressions.leaves().active.empty());
}

TEST(Expressions, MixedPayloadIndicesWaitForFutureEntityDestruction) {
    SimClock clock;
    clock.initialise({.tick_rate = Frequency::make(10.0)});
    EntityLedger ledger;
    ExprManager expressions;
    Mission mission;
    std::array<EntityId, 1> entities;
    std::array const nodes{timer(1),
                           MissionExpr{EntityDestroyedExpr{levels::EntityId{"future"}}},
                           timer(2),
                           MissionExpr{AllExpr{.children = {0, 1, 2}}}};
    auto const compiled{expressions.append(nodes, {{{levels::EntityId{"future"}, 0}}, {}, clock})};
    ASSERT_TRUE(compiled);
    auto const& handles{compiled->handles};
    expressions.activate(handles[3], SimTick{0});
    expressions.evaluate({SimTick{10}, ledger, entities}, mission);
    EXPECT_EQ(expressions.result(handles[0]), ExprResult::True);
    EXPECT_EQ(expressions.result(handles[2]), ExprResult::Pending);
    expressions.evaluate({SimTick{20}, ledger, entities}, mission);
    EXPECT_EQ(expressions.result(handles[2]), ExprResult::True);
    EXPECT_EQ(expressions.result(handles[3]), ExprResult::Pending);

    ledger.record_spawns(EntityType::Fighter, Team::Red, Health{1}, entities);
    expressions.evaluate({SimTick{21}, ledger, entities}, mission);
    EXPECT_EQ(expressions.result(handles[1]), ExprResult::Pending);
    EntityDeathInfo deaths;
    deaths.add_defaulted(1);
    deaths.view().victims()[0] = entities[0];
    deaths.view().reasons()[0] = DeathReason::Combat;
    ledger.record_deaths(deaths.const_view(), SimTick{22});
    expressions.evaluate({SimTick{22}, ledger, entities}, mission);
    EXPECT_EQ(expressions.result(handles[3]), ExprResult::True);
}
}
