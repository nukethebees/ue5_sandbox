#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/expressions/expr_manager.h>
#include <ioj/sim/expressions/timers.h>
#include <ioj/sim/missions/mission.h>

#include <gtest/gtest.h>

#include <array>

namespace ioj::sim::expression_tests {
using namespace ioj::levels;

auto timer() -> MissionExpr {
    return {TimeElapsedExpr{}};
}

TEST(Expressions, SharedDagEvaluatesChildrenFirstWithoutRetiringSharedLeaves) {
    EntityLedger ledger;
    ExprManager expressions;
    Timers timers;
    auto const first{timers.add()};
    auto const second{timers.add()};
    Mission mission;
    std::array const nodes{MissionExpr{AllExpr{.children = {1, 2}}},
                           MissionExpr{AllExpr{.children = {3, 3, 4}}},
                           MissionExpr{AnyExpr{.children = {3, 4}}},
                           timer(),
                           timer()};
    std::array const timer_handles{TimerHandle{}, TimerHandle{}, TimerHandle{}, first, second};
    auto const compiled{expressions.append(nodes, {{}, {}, timer_handles})};
    auto const& handles{compiled.handles};
    expressions.activate(handles[0]);
    timers.start(first, SimTick{10});
    timers.start(second, SimTick{20});
    EXPECT_EQ(expressions.leaves().active.size(), 2);

    timers.tick(SimTick{10});
    expressions.evaluate({ledger, timers, {}}, mission);
    EXPECT_EQ(expressions.result(handles[2]), ExprResult::True);
    EXPECT_EQ(expressions.result(handles[4]), ExprResult::Pending);
    EXPECT_EQ(expressions.result(handles[0]), ExprResult::Pending);
    EXPECT_EQ(expressions.leaves().active.size(), 1);

    timers.tick(SimTick{10});
    expressions.evaluate({ledger, timers, {}}, mission);
    EXPECT_EQ(expressions.result(handles[0]), ExprResult::True);
    EXPECT_TRUE(expressions.leaves().active.empty());
}

TEST(Expressions, MixedPayloadIndicesWaitForFutureEntityDestruction) {
    EntityLedger ledger;
    ExprManager expressions;
    Timers timers;
    auto const first{timers.add()};
    auto const second{timers.add()};
    Mission mission;
    std::array<EntityId, 1> entities;
    std::array const nodes{timer(),
                           MissionExpr{EntityDestroyedExpr{levels::EntityId{"future"}}},
                           timer(),
                           MissionExpr{AllExpr{.children = {0, 1, 2}}}};
    std::array const timer_handles{first, TimerHandle{}, second, TimerHandle{}};
    auto const compiled{
        expressions.append(nodes, {{{levels::EntityId{"future"}, 0}}, {}, timer_handles})};
    auto const& handles{compiled.handles};
    expressions.activate(handles[3]);
    timers.start(first, SimTick{10});
    timers.start(second, SimTick{20});
    timers.tick(SimTick{10});
    expressions.evaluate({ledger, timers, entities}, mission);
    EXPECT_EQ(expressions.result(handles[0]), ExprResult::True);
    EXPECT_EQ(expressions.result(handles[2]), ExprResult::Pending);
    timers.tick(SimTick{10});
    expressions.evaluate({ledger, timers, entities}, mission);
    EXPECT_EQ(expressions.result(handles[2]), ExprResult::True);
    EXPECT_EQ(expressions.result(handles[3]), ExprResult::Pending);

    ledger.record_spawns(EntityType::Fighter, Team::Red, Health{1}, entities);
    timers.tick(SimTick{1});
    expressions.evaluate({ledger, timers, entities}, mission);
    EXPECT_EQ(expressions.result(handles[1]), ExprResult::Pending);
    EntityDeathInfo deaths;
    deaths.add_defaulted(1);
    deaths.view().victims()[0] = entities[0];
    deaths.view().reasons()[0] = DeathReason::Combat;
    ledger.record_deaths(deaths.const_view(), SimTick{22});
    timers.tick(SimTick{1});
    expressions.evaluate({ledger, timers, entities}, mission);
    EXPECT_EQ(expressions.result(handles[3]), ExprResult::True);
}

TEST(Expressions, ActivationDoesNotStartTimersAndStartIsVisibleOnNextEvaluation) {
    EntityLedger ledger;
    ExprManager expressions;
    Timers timers;
    Mission mission;
    auto const first{timers.add()};
    auto const second{timers.add()};
    auto const zero{expressions.add_timer_elapsed(first)};
    auto const delayed{expressions.add_timer_elapsed(second)};
    expressions.activate(zero);
    expressions.activate(delayed);
    timers.tick(SimTick{100});
    expressions.evaluate({ledger, timers, {}}, mission);
    EXPECT_EQ(expressions.result(zero), ExprResult::Pending);
    EXPECT_EQ(expressions.result(delayed), ExprResult::Pending);

    timers.start(first, SimTick{});
    timers.start(second, SimTick{3});
    EXPECT_EQ(expressions.result(zero), ExprResult::Pending);
    timers.tick(SimTick{1});
    EXPECT_EQ(expressions.result(zero), ExprResult::Pending);
    expressions.evaluate({ledger, timers, {}}, mission);
    EXPECT_EQ(expressions.result(zero), ExprResult::True);
    EXPECT_EQ(expressions.result(delayed), ExprResult::Pending);
    expressions.deactivate(delayed);
    expressions.activate(delayed);
    timers.tick(SimTick{1});
    expressions.evaluate({ledger, timers, {}}, mission);
    EXPECT_EQ(expressions.result(delayed), ExprResult::Pending);
    timers.tick(SimTick{1});
    expressions.evaluate({ledger, timers, {}}, mission);
    EXPECT_EQ(expressions.result(delayed), ExprResult::True);
}
}
