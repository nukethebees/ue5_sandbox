#include <ioj/frame_array.h>
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
static_assert(sizeof(ObjectiveId) == sizeof(std::uint16_t));
static_assert(sizeof(ObjectivePrerequisite) == sizeof(std::uint32_t));
static_assert(!std::convertible_to<std::uint16_t, ObjectiveId>);
static_assert(!std::convertible_to<EntityId, ObjectiveId>);
static_assert(sizeof(ExprHandle) == sizeof(std::uint32_t));
static_assert(sizeof(LeafDataHandle) == sizeof(std::uint32_t));

TEST(MissionAuthoring, RejectsInvalidConstrainedInputsBeforeRuntimeCompilation) {
    for (auto const expression : {"(time-elapsed 1)",
                                  "(time-elapsed -1)",
                                  "(time-elapsed +inf.0)",
                                  "(time-elapsed +nan.0)",
                                  "(entity-reached-position 'player '(0 +inf.0 0) 1)",
                                  "(entity-reached-position 'player '(0 0 0) -1)"}) {
        auto const source{std::string{R"((level :id 'invalid :title "Invalid" :player 'player
          :entities (list (entity :id 'player :archetype 'player-fighter :team 'blue :position '(0 0 0)))
          :mission (mission :objectives (list (objective :id 'a :team 'blue :pass )"} +
                          expression + "))))"};
        auto const parsed{levels::authoring::DefinitionReader{}.read_level_source(source)};
        EXPECT_EQ(parsed.has_value(), std::string_view{expression} == "(time-elapsed 1)")
            << expression;
    }
}

namespace {
struct MissionTest : ::testing::Test {
    SimClock clock;
    EntityLedger ledger;
    Mission mission;
    ExprManager expressions;
    std::vector<levels::EntityId> names;
    std::vector<EntityId> entities;
    std::array<Team, 2> teams{Team::Blue, Team::Red};
    FrameMemoryResource scratch{4096};

    MissionTest() { clock.initialise({.tick_rate = Frequency::make(10.0)}); }
    void start(MissionDefinition const& definition) {
        expressions = ExprManager{};
        auto compiled{Mission::compile(definition, names, clock, expressions)};
        ASSERT_TRUE(compiled.has_value()) << (compiled ? "" : compiled.error().front());
        mission = std::move(*compiled);
        mission.reset_progress();
        expressions.reset_progress();
    }
    void tick(SimTick::value_type const tick) {
        FrameScratchScope scratch_scope{scratch};
        expressions.evaluate({SimTick{tick}, ledger, entities, nullptr, &scratch}, mission);
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

TEST_F(MissionTest, SharedDagEvaluatesChildrenFirstAndCompletedObjectivesAreTerminal) {
    auto source{objective("shared", timer(1))};
    source.expressions = {{AllExpr{.children = {1, 2}}},
                          {AllExpr{.children = {3, 3, 4}}},
                          {AnyExpr{.children = {3, 4}}},
                          timer(1),
                          timer(2)};
    start({.objectives = {source}});
    tick(0);
    tick(10);
    EXPECT_FALSE(mission.outcome(ObjectiveId{0}, expressions));
    EXPECT_EQ(mission.active_objectives().size(), 1);
    tick(10);
    EXPECT_FALSE(mission.outcome(ObjectiveId{0}, expressions));
    tick(20);
    EXPECT_EQ(mission.outcome(ObjectiveId{0}, expressions), ObjectiveOutcome::Passed);
    EXPECT_TRUE(mission.active_objectives().empty());
    tick(100);
    EXPECT_EQ(mission.completed_objectives().size(), 1);
}

TEST_F(MissionTest, FalseOutcomeLeavesShortCircuitAndAndResolveOrOnlyAfterAllFalse) {
    auto a{objective("a", timer(1), ObjectiveOutcome::Failed)};
    auto b{objective("b", timer(2), ObjectiveOutcome::Failed)};
    auto conjunction{objective("and", observed("a", ObjectiveOutcome::Passed))};
    conjunction.expressions.push_back(observed("b", ObjectiveOutcome::Passed));
    conjunction.expressions.push_back({AllExpr{.children = {0, 1}}});
    conjunction.roots[ioj::ObjectiveOutcome::Passed] = 2;
    auto disjunction{conjunction};
    disjunction.id = levels::ObjectiveName{"or"};
    disjunction.expressions[2] = {AnyExpr{.children = {0, 1}}};
    auto compiled{
        Mission::compile({.objectives = {a, b, conjunction, disjunction}}, {}, clock, expressions)};
    ASSERT_TRUE(compiled);
    auto const and_root{
        compiled->objectives().roots(ObjectiveId{2})[ioj::ObjectiveOutcome::Passed]};
    auto const or_root{compiled->objectives().roots(ObjectiveId{3})[ioj::ObjectiveOutcome::Passed]};
    mission = std::move(*compiled);
    mission.reset_progress();
    expressions.reset_progress();
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

TEST_F(MissionTest, ShortCircuitDoesNotDisableSharedLeaf) {
    auto source{objective("shared", timer(0))};
    source.expressions.push_back(timer(2));
    source.expressions.push_back({AnyExpr{.children = {0, 1}}});
    source.expressions.push_back({AllExpr{.children = {1, 2}}});
    source.roots[ioj::ObjectiveOutcome::Passed] = 3;
    start({.objectives = {source}});
    tick(0);
    EXPECT_FALSE(mission.outcome(ObjectiveId{0}, expressions));
    tick(20);
    EXPECT_EQ(mission.outcome(ObjectiveId{0}, expressions), ObjectiveOutcome::Passed);
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

TEST_F(MissionTest, BypassExpiresOnlyExplicitObservers) {
    auto bypass{objective("bypass", timer(1))};
    auto a{objective("a", observed("bypass", ObjectiveOutcome::Passed), ObjectiveOutcome::Expired)};
    auto b{a};
    b.id = levels::ObjectiveName{"b"};
    auto unrelated{objective("unrelated", timer(10))};
    start({.objectives = {a, b, bypass, unrelated}});
    tick(0);
    tick(10);
    EXPECT_FALSE(mission.outcome(ObjectiveId{0}, expressions));
    EXPECT_FALSE(mission.outcome(ObjectiveId{1}, expressions));
    tick(11);
    EXPECT_EQ(mission.outcome(ObjectiveId{0}, expressions), ObjectiveOutcome::Expired);
    EXPECT_EQ(mission.outcome(ObjectiveId{1}, expressions), ObjectiveOutcome::Expired);
    EXPECT_FALSE(mission.outcome(ObjectiveId{3}, expressions));
    EXPECT_EQ(mission.active_objectives().size(), 1);
}

TEST_F(MissionTest, ReciprocalTeamCompletionsPersistUntilProgressResets) {
    auto attack{objective("attack", timer(1))};
    auto defend{objective(
        "defend", observed("attack", ObjectiveOutcome::Passed), ObjectiveOutcome::Failed)};
    defend.team = Team::Red;
    attack.wins_level = ObjectiveWinsLevel::Yes;
    start({.objectives = {attack, defend}});
    tick(0);
    tick(10);
    auto const completion{mission.team_completion(Team::Blue)};
    ASSERT_TRUE(completion);
    EXPECT_EQ(completion->outcome, MissionOutcome::Victory);
    auto const defeat{mission.team_completion(Team::Red)};
    ASSERT_TRUE(defeat);
    EXPECT_EQ(defeat->outcome, MissionOutcome::Defeat);
    tick(20);
    EXPECT_TRUE(mission.team_completion(Team::Blue));
    EXPECT_EQ(mission.completions().size(), 2);

    mission.reset_progress();
    expressions.reset_progress();
    EXPECT_TRUE(mission.completions().empty());
    EXPECT_FALSE(mission.team_completion(Team::Blue));
    tick(30);
    tick(40);
    auto const restarted{mission.team_completion(Team::Blue)};
    ASSERT_TRUE(restarted);
    EXPECT_EQ(restarted->tick, SimTick{40});
}

struct TeamMissionCompletionCase {
    char const* name;
    std::optional<Team> team;
    std::optional<MissionOutcome> outcome;
};

class SelectedTeamMissionCompletion
    : public MissionTest
    , public ::testing::WithParamInterface<TeamMissionCompletionCase> {};

TEST_P(SelectedTeamMissionCompletion, ReadsTheRequestedTeamWithoutConsumingResults) {
    auto attack{objective("attack", timer(1))};
    auto defend{objective(
        "defend", observed("attack", ObjectiveOutcome::Passed), ObjectiveOutcome::Failed)};
    defend.team = Team::Red;
    attack.wins_level = ObjectiveWinsLevel::Yes;
    start({.objectives = {attack, defend}});
    tick(0);
    auto const& test_case{GetParam()};
    EXPECT_FALSE(mission.team_completion(test_case.team));
    tick(10);

    auto const completion{mission.team_completion(test_case.team)};
    ASSERT_EQ(completion.has_value(), test_case.outcome.has_value());
    if (completion) {
        EXPECT_EQ(completion->team, *test_case.team);
        EXPECT_EQ(completion->outcome, *test_case.outcome);
        EXPECT_EQ(completion->tick, SimTick{10});
    }
    EXPECT_TRUE(mission.team_completion(Team::Blue));
    EXPECT_TRUE(mission.team_completion(Team::Red));
    EXPECT_EQ(mission.completions().size(), 2);
    tick(20);
    EXPECT_EQ(mission.team_completion(test_case.team).has_value(), test_case.outcome.has_value());
    EXPECT_EQ(mission.completions().size(), 2);
}

INSTANTIATE_TEST_SUITE_P(
    Mission,
    SelectedTeamMissionCompletion,
    ::testing::Values(
        TeamMissionCompletionCase{"VictoriousTeam", Team::Blue, MissionOutcome::Victory},
        TeamMissionCompletionCase{"DefeatedTeam", Team::Red, MissionOutcome::Defeat},
        TeamMissionCompletionCase{"UnrelatedTeam", Team::Green, std::nullopt},
        TeamMissionCompletionCase{"NoPlayer", std::nullopt, std::nullopt}),
    [](::testing::TestParamInfo<TeamMissionCompletionCase> const& info) {
        return info.param.name;
    });

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
}

TEST_F(MissionTest, MixedLeafKindsProgressUntilTheLastConditionResolves) {
    names.emplace_back("future");
    entities.emplace_back();
    auto source{objective("leaves", timer(1))};
    source.expressions.push_back({EntityDestroyedExpr{.entity = names[0]}});
    source.expressions.push_back(timer(2));
    source.expressions.push_back({AllExpr{.children = {0, 1, 2}}});
    source.roots[ioj::ObjectiveOutcome::Passed] = 3;
    start({.objectives = {source}});
    tick(0);
    auto const root{mission.objectives().roots(ObjectiveId{0})[ObjectiveOutcome::Passed]};
    EXPECT_EQ(expressions.result(root), ExprResult::Pending);
    tick(10);
    EXPECT_EQ(expressions.result(root), ExprResult::Pending);
    EXPECT_FALSE(mission.outcome(ObjectiveId{0}, expressions));
    tick(20);
    EXPECT_EQ(expressions.result(root), ExprResult::Pending);
    EXPECT_FALSE(mission.outcome(ObjectiveId{0}, expressions));

    ledger.record_spawns(EntityType::Fighter, Team::Red, Health{1}, entities);
    tick(21);
    EXPECT_FALSE(mission.outcome(ObjectiveId{0}, expressions));

    EntityDeathInfo deaths;
    deaths.add_defaulted(1);
    auto const death{deaths.view()};
    death.victims()[0] = entities[0];
    death.reasons()[0] = DeathReason::Combat;
    ledger.record_deaths(deaths.const_view(), SimTick{22});
    tick(22);
    EXPECT_EQ(expressions.result(root), ExprResult::True);
    EXPECT_EQ(mission.outcome(ObjectiveId{0}, expressions), ObjectiveOutcome::Passed);
}

TEST_F(MissionTest, RejectsStructuralAndActivationCyclesButAllowsObservationCycles) {
    auto a{objective("a", observed("b", ObjectiveOutcome::Passed))};
    auto b{objective("b", observed("a", ObjectiveOutcome::Passed))};
    EXPECT_TRUE(Mission::compile({.objectives = {a, b}}, {}, clock, expressions));
    a.dependencies = {{levels::ObjectiveName{"b"}, ObjectiveOutcome::Passed}};
    b.dependencies = {{levels::ObjectiveName{"a"}, ObjectiveOutcome::Passed}};
    EXPECT_FALSE(Mission::compile({.objectives = {a, b}}, {}, clock, expressions));
    a.dependencies.clear();
    a.expressions = {{AllExpr{.children = {1}}}, {AnyExpr{.children = {0}}}};
    EXPECT_FALSE(Mission::compile({.objectives = {a}}, {}, clock, expressions));
    std::get<AnyExpr>(a.expressions[1].payload).children = {99};
    EXPECT_FALSE(Mission::compile({.objectives = {a}}, {}, clock, expressions));
}

TEST_F(MissionTest, SchemeNamedDagAndOutcomeDependenciesExecuteThroughRuntimeCompiler) {
    auto const parsed{levels::authoring::DefinitionReader{}.read_level_source(R"(
      (level :id 'objectives :title "Objectives" :player 'player
        :entities (list (entity :id 'player :archetype 'player-fighter :team 'blue :position '(0 0 0)))
        :mission (mission
          :objectives (list
            (objective :id 'first :team 'blue
              :nodes (list (expr 'timer (time-elapsed 1)))
              :pass (all-of (expr-ref 'timer) (any-of (expr-ref 'timer) (time-elapsed 100))))
            (objective :id 'next :team 'blue :wins-level 'yes
              :depends-on (list (objective-resolved 'first 'passed))
              :pass (all-of (objective-resolved 'first 'passed) (time-elapsed 2)))))))")};
    ASSERT_TRUE(parsed);
    ASSERT_TRUE(parsed->mission());
    names.emplace_back("player");
    entities.emplace_back();
    start(*parsed->mission());
    tick(0);
    tick(10);
    EXPECT_EQ(mission.outcome(ObjectiveId{0}, expressions), ObjectiveOutcome::Passed);
    EXPECT_FALSE(mission.team_completion(Team::Blue));
    tick(29);
    EXPECT_FALSE(mission.outcome(ObjectiveId{1}, expressions));
    tick(30);
    auto const completion{mission.team_completion(Team::Blue)};
    ASSERT_TRUE(completion);
    EXPECT_EQ(completion->outcome, MissionOutcome::Victory);
}

TEST(MissionAuthoring, RejectsNamedDagCyclesAndCrossObjectiveExpressionReferences) {
    for (auto const expression : {"(expr-ref 'missing)", "(expr-ref 'loop)"}) {
        auto const source{std::string{R"((level :id 'cycle :title "Cycle" :player 'player
          :entities (list (entity :id 'player :archetype 'player-fighter :team 'blue :position '(0 0 0)))
          :mission (mission :objectives (list (objective :id 'a :team 'blue
            :nodes (list (expr 'loop )"} +
                          expression + ")) :pass (expr-ref 'loop))))))"};
        EXPECT_FALSE(levels::authoring::DefinitionReader{}.read_level_source(source));
    }
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

TEST_F(MissionTest, RejectsInvalidReferencesParametersAndDuplicatePrerequisites) {
    auto a{objective("a", timer(0))};
    auto b{objective("b", timer(1))};
    b.dependencies = {{levels::ObjectiveName{"a"}, ObjectiveOutcome::Passed},
                      {levels::ObjectiveName{"a"}, ObjectiveOutcome::Passed}};
    EXPECT_FALSE(Mission::compile({.objectives = {a, b}}, {}, clock, expressions));
    EXPECT_FALSE(Mission::compile({.objectives = {a, a}}, {}, clock, expressions));
    b.dependencies = {{levels::ObjectiveName{"missing"}, ObjectiveOutcome::Passed}};
    EXPECT_FALSE(Mission::compile({.objectives = {a, b}}, {}, clock, expressions));
    a.expressions[0] = {EntityReachedPositionExpr{
        .entity = levels::EntityId{"target"},
        .radius = NonNegativeFinite<double>::make(std::numeric_limits<double>::max())}};
    names.emplace_back("target");
    EXPECT_FALSE(Mission::compile({.objectives = {a}}, names, clock, expressions));
}
}
