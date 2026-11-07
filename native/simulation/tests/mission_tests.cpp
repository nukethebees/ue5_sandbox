#include <ioj/frame_memory_resource.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/mission_validation.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/expressions/expr_manager.h>
#include <ioj/sim/missions/mission.h>
#include <ioj/sim/sim_clock.h>

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <optional>

namespace ioj::sim::tests {
using namespace ioj::levels;
namespace {
struct MissionTest : ::testing::Test {
    SimClock clock;
    EntityLedger ledger;
    Mission mission;
    ExprManager expressions;
    std::array<Team, 2> teams{Team::Blue, Team::Red};
    FrameMemoryResource scratch{4096};

    MissionTest() { clock.initialise({.tick_rate = Frequency::make(10.0)}); }
    void start(MissionDefinition const& definition) {
        expressions = ExprManager{};
        auto compiled{Mission::compile(definition, {}, clock, expressions)};
        ASSERT_TRUE(compiled.has_value()) << (compiled ? "" : compiled.error().front());
        mission = std::move(*compiled);
        mission.reset_progress();
        expressions.reset_progress();
    }
    void tick(SimTick::value_type const tick) {
        FrameScratchScope scratch_scope{scratch};
        expressions.evaluate({SimTick{tick}, ledger, {}, nullptr, &scratch}, mission);
        auto const changes{mission.evaluate({SimTick{tick}, &scratch, teams}, expressions)};
        auto const& objectives{mission.objectives()};
        for (auto const id : changes.completed) {
            expressions.deactivate(objectives.leaves(id), objectives.composites(id));
        }
        for (auto const id : changes.activated) {
            expressions.activate(objectives.leaves(id), objectives.composites(id), SimTick{tick});
        }
    }
};
auto timer(double const seconds) -> MissionExpr {
    return {TimeElapsedExpr{.seconds = NonNegativeFinite<double>::make(seconds)}};
}
auto observed(std::string id, ObjectiveOutcome const outcome) -> MissionExpr {
    return {ObjectiveResolvedExpr{.objective = {levels::ObjectiveName{std::move(id)}, outcome}}};
}
auto objective(std::string id,
               MissionExpr expression,
               ObjectiveOutcome const outcome = ObjectiveOutcome::Passed) -> ObjectiveDefinition {
    ObjectiveDefinition result{.id = levels::ObjectiveName{std::move(id)}, .team = Team::Blue};
    result.expressions.push_back(std::move(expression));
    result.roots[outcome] = 0;
    return result;
}
}

TEST_F(MissionTest, ObserversSeePriorTickOutcomesAndPropagateFalse) {
    auto a{objective("a", timer(1), ObjectiveOutcome::Failed)};
    auto b{objective("b", timer(2), ObjectiveOutcome::Failed)};
    auto conjunction{objective("and", observed("a", ObjectiveOutcome::Passed))};
    conjunction.expressions.push_back(observed("b", ObjectiveOutcome::Passed));
    conjunction.expressions.push_back({AllExpr{.children = {0, 1}}});
    conjunction.roots[ioj::ObjectiveOutcome::Passed] = 2;
    auto disjunction{conjunction};
    disjunction.id = levels::ObjectiveName{"or"};
    disjunction.expressions[2] = {AnyExpr{.children = {0, 1}}};
    start({.objectives = {a, b, conjunction, disjunction}});
    auto const and_root{mission.objectives().roots(ObjectiveId{2})[ObjectiveOutcome::Passed]};
    auto const or_root{mission.objectives().roots(ObjectiveId{3})[ObjectiveOutcome::Passed]};
    tick(0);
    tick(10);
    EXPECT_EQ(expressions.result(and_root), ExprResult::Pending);
    tick(11);
    EXPECT_EQ(expressions.result(and_root), ExprResult::False);
    EXPECT_EQ(expressions.result(or_root), ExprResult::Pending);
    tick(20);
    EXPECT_EQ(expressions.result(or_root), ExprResult::Pending);
    tick(21);
    EXPECT_EQ(expressions.result(or_root), ExprResult::False);
    EXPECT_FALSE(mission.outcome(ObjectiveId{2}, expressions));
    EXPECT_FALSE(mission.outcome(ObjectiveId{3}, expressions));
}

TEST_F(MissionTest, ActivationRelativeTimersAndOutcomeBranches) {
    auto first{objective("first", timer(1))};
    auto next{objective("next", timer(2))};
    next.dependencies.push_back({levels::ObjectiveName{"first"}, ObjectiveOutcome::Passed});
    auto other{objective("other", timer(0))};
    other.dependencies.push_back({levels::ObjectiveName{"first"}, ObjectiveOutcome::Failed});
    start({.objectives = {first, next, other}});
    tick(0);
    EXPECT_EQ(mission.inactive_objectives().size(), 2);
    tick(10);
    EXPECT_EQ(mission.active_objectives().size(), 1);
    tick(29);
    EXPECT_FALSE(mission.outcome(ObjectiveId{1}, expressions));
    tick(30);
    EXPECT_EQ(mission.outcome(ObjectiveId{1}, expressions), ObjectiveOutcome::Passed);
    EXPECT_FALSE(mission.outcome(ObjectiveId{2}, expressions));
    EXPECT_EQ(mission.inactive_objectives().size(), 1);
}

TEST_F(MissionTest, AllPrerequisitesActivateBeforeNextTickEvaluation) {
    auto first{objective("first", timer(1))};
    auto second{objective("second", timer(2), ObjectiveOutcome::Expired)};
    auto third{objective("third", observed("first", ObjectiveOutcome::Passed))};
    third.dependencies = {{levels::ObjectiveName{"first"}, ObjectiveOutcome::Passed},
                          {levels::ObjectiveName{"second"}, ObjectiveOutcome::Expired}};
    start({.objectives = {first, second, third}});
    tick(0);
    tick(10);
    EXPECT_EQ(mission.inactive_objectives().size(), 1);
    tick(20);
    EXPECT_FALSE(mission.outcome(ObjectiveId{2}, expressions));
    EXPECT_TRUE(mission.inactive_objectives().empty());
    tick(21);
    EXPECT_EQ(mission.outcome(ObjectiveId{2}, expressions), ObjectiveOutcome::Passed);
    EXPECT_TRUE(mission.active_objectives().empty());
}

TEST_F(MissionTest, TeamCompletionsPersistUntilProgressResets) {
    auto winner{objective("winner", timer(1))};
    winner.wins_level = ObjectiveWinsLevel::Yes;
    start({.objectives = {winner}});
    EXPECT_FALSE(mission.team_completion(Team::Blue));
    tick(0);
    tick(10);
    auto const victory{mission.team_completion(Team::Blue)};
    auto const defeat{mission.team_completion(Team::Red)};
    ASSERT_TRUE(victory);
    ASSERT_TRUE(defeat);
    EXPECT_EQ(victory->outcome, MissionOutcome::Victory);
    EXPECT_EQ(defeat->outcome, MissionOutcome::Defeat);
    EXPECT_EQ(victory->tick, SimTick{10});
    EXPECT_FALSE(mission.team_completion(Team::Green));
    EXPECT_FALSE(mission.team_completion(std::nullopt));
    tick(20);
    ASSERT_TRUE(mission.team_completion(Team::Blue));
    EXPECT_EQ(mission.team_completion(Team::Blue)->tick, victory->tick);
    EXPECT_EQ(mission.completions().size(), 2);

    mission.reset_progress();
    expressions.reset_progress();
    EXPECT_FALSE(mission.mission_completed());
    EXPECT_TRUE(mission.completions().empty());
    tick(30);
    tick(39);
    EXPECT_FALSE(mission.team_completion(Team::Blue));
    EXPECT_FALSE(mission.outcome(ObjectiveId{0}, expressions));
    tick(40);
    ASSERT_TRUE(mission.team_completion(Team::Blue));
    EXPECT_EQ(mission.team_completion(Team::Blue)->tick, SimTick{40});
    EXPECT_TRUE(mission.mission_completed());
}

TEST_F(MissionTest, RootAndTeamConflictPrecedenceIsDeterministic) {
    auto source{objective("conflict", timer(0))};
    source.roots = levels::ObjectiveRoots{std::array<levels::ExprIndex, 3>{0, 0, 0}};
    source.wins_level = ObjectiveWinsLevel::Yes;
    auto rival{objective("rival", timer(0))};
    rival.team = Team::Red;
    rival.wins_level = ObjectiveWinsLevel::Yes;
    start({.objectives = {source, rival}});
    EXPECT_TRUE(mission.active_objectives().empty());
    EXPECT_TRUE(mission.completed_objectives().empty());
    EXPECT_FALSE(mission.team_completion(Team::Blue));
    tick(0);
    EXPECT_EQ(mission.active_objectives().size(), 2);
    EXPECT_TRUE(mission.completed_objectives().empty());
    tick(1);
    EXPECT_EQ(mission.outcome(ObjectiveId{0}, expressions), ObjectiveOutcome::Passed);
    EXPECT_EQ(mission.outcome(ObjectiveId{1}, expressions), ObjectiveOutcome::Passed);
    auto const completion{mission.team_completion(Team::Blue)};
    ASSERT_TRUE(completion);
    EXPECT_EQ(completion->outcome, MissionOutcome::Victory);
    EXPECT_EQ(completion->triggering_objective, ObjectiveId{0});
    tick(10);
    EXPECT_TRUE(mission.team_completion(Team::Blue));
    EXPECT_EQ(mission.completions().size(), 2);
    EXPECT_EQ(mission.completed_objectives().size(), 2);
}

TEST_F(MissionTest, SchemeReciprocalHelperResolvesBothTeams) {
    auto const parsed{levels::authoring::DefinitionReader{
        std::filesystem::path{IOJ_PROJECT_SOURCE_DIR} / "LevelScripts"}
                          .read_level_source(R"(
      (load-script "Libraries/objectives.scm")
      (level :id 'reciprocal :title "Reciprocal" :player 'player
        :entities (list (entity :id 'player :archetype 'player-fighter :team 'blue :position '(0 0 0)))
        :mission (mission
          :objectives (reciprocal-objectives 'attack 'defend 'blue 'red
                         (time-elapsed 1) (time-elapsed 10)))))")};
    ASSERT_TRUE(parsed);
    ASSERT_TRUE(parsed->mission());
    start(*parsed->mission());
    tick(0);
    tick(10);
    EXPECT_EQ(mission.outcome(ObjectiveId{0}, expressions), ObjectiveOutcome::Passed);
    EXPECT_FALSE(mission.outcome(ObjectiveId{1}, expressions));
    tick(11);
    EXPECT_EQ(mission.outcome(ObjectiveId{1}, expressions), ObjectiveOutcome::Failed);
    auto const completion{mission.team_completion(Team::Red)};
    ASSERT_TRUE(completion);
    EXPECT_EQ(completion->outcome, MissionOutcome::Defeat);
}

TEST(MissionValidation, DistinguishesObservationFromActivationCyclesAndRejectsInvalidReferences) {
    auto a{objective("a", observed("b", ObjectiveOutcome::Passed))};
    auto b{objective("b", observed("a", ObjectiveOutcome::Passed))};
    EXPECT_TRUE(validate_mission({.objectives = {a, b}}, {}).empty());
    a.dependencies = {{ObjectiveName{"b"}, ObjectiveOutcome::Passed}};
    b.dependencies = {{ObjectiveName{"a"}, ObjectiveOutcome::Passed}};
    EXPECT_FALSE(validate_mission({.objectives = {a, b}}, {}).empty());

    a = objective("a", timer(0));
    b.dependencies = {{ObjectiveName{"a"}, ObjectiveOutcome::Passed},
                      {ObjectiveName{"a"}, ObjectiveOutcome::Passed}};
    EXPECT_FALSE(validate_mission({.objectives = {a, b}}, {}).empty());
    EXPECT_FALSE(validate_mission({.objectives = {a, a}}, {}).empty());
    b.dependencies = {{ObjectiveName{"missing"}, ObjectiveOutcome::Passed}};
    EXPECT_FALSE(validate_mission({.objectives = {a, b}}, {}).empty());
    a.expressions = {{AllExpr{.children = {99}}}};
    EXPECT_FALSE(validate_mission({.objectives = {a}}, {}).empty());

    a.expressions = {{EntityReachedPositionExpr{
        .entity = levels::EntityId{"target"},
        .radius = NonNegativeFinite<double>::make(std::numeric_limits<double>::max())}}};
    std::array const names{levels::EntityId{"target"}};
    EXPECT_FALSE(validate_mission({.objectives = {a}}, names).empty());
    SimClock clock;
    ExprManager expressions;
    EXPECT_FALSE(Mission::compile({.objectives = {a}}, names, clock, expressions));
}
}
