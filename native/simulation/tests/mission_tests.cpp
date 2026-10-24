#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/mission_validation.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/missions/mission.h>
#include <ioj/sim/missions/mission_compilation.h>

#include <gtest/gtest.h>

#include <limits>

namespace ioj::sim::tests {
static_assert(sizeof(ObjectiveId) == sizeof(std::uint32_t));
static_assert(!std::convertible_to<std::uint32_t, ObjectiveId>);
static_assert(!std::convertible_to<EntityId, ObjectiveId>);
static_assert(sizeof(ExprRef) == sizeof(std::uint32_t));
static_assert(sizeof(RuleId) == sizeof(std::uint32_t));

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
struct MissionTest : testing::Test {
    SimClock clock;
    EntityLedger ledger;
    Mission mission;
    std::vector<levels::EntityId> names;
    std::vector<EntityId> entities;

    MissionTest() { clock.initialise({.tick_rate = Frequency::make(10.0)}); }
    void start(levels::MissionDefinition const& definition) {
        auto compiled{compile_mission(definition, names, clock)};
        ASSERT_TRUE(compiled.has_value()) << (compiled ? "" : compiled.error().front());
        mission = Mission{std::move(*compiled)};
        mission.begin({0, ledger, entities});
    }
    void tick(SimTick const tick) { mission.tick({tick, ledger, entities}); }
};
auto timer(double const seconds) -> levels::MissionExpression {
    return {ioj::levels::TimeElapsedExpression{.seconds =
                                                   ioj::NonNegativeFinite<double>::make(seconds)}};
}
auto observed(std::string id, ObjectiveOutcome const outcome) -> levels::MissionExpression {
    return {ioj::levels::ObjectiveResolvedExpression{
        .objective = {ioj::levels::ObjectiveName{std::move(id)}, outcome}}};
}
auto objective(std::string id,
               levels::MissionExpression expression,
               ObjectiveOutcome const outcome = ObjectiveOutcome::Passed)
    -> levels::ObjectiveDefinition {
    levels::ObjectiveDefinition result{.id = ioj::levels::ObjectiveName{std::move(id)},
                                       .team = Team::Blue};
    result.expressions.push_back(std::move(expression));
    result.roots[outcome] = 0;
    return result;
}
}

TEST_F(MissionTest, SharedDagPropagatesOnceAndCompletedObjectivesAreTerminal) {
    auto source{objective("shared", timer(1))};
    source.expressions.push_back(timer(2));
    source.expressions.push_back({ioj::levels::AllExpression{.children = {0, 0, 1}}});
    source.expressions.push_back({ioj::levels::AnyExpression{.children = {0, 1}}});
    source.expressions.push_back({ioj::levels::AllExpression{.children = {2, 3}}});
    source.roots[ioj::ObjectiveOutcome::Passed] = 4;
    start({.objectives = {source}});
    tick(10);
    EXPECT_FALSE(mission.outcome(ioj::sim::ObjectiveId{0}));
    EXPECT_EQ(mission.active_rules().size(), 1);
    tick(10);
    EXPECT_FALSE(mission.outcome(ioj::sim::ObjectiveId{0}));
    tick(20);
    EXPECT_EQ(mission.outcome(ioj::sim::ObjectiveId{0}), ObjectiveOutcome::Passed);
    EXPECT_TRUE(mission.active_rules().empty());
    tick(100);
    EXPECT_EQ(mission.completed_objectives().size(), 1);
}

TEST_F(MissionTest, FalseOutcomeLeavesShortCircuitAndAndResolveOrOnlyAfterAllFalse) {
    auto a{objective("a", timer(1), ObjectiveOutcome::Failed)};
    auto b{objective("b", timer(2), ObjectiveOutcome::Failed)};
    auto conjunction{objective("and", observed("a", ObjectiveOutcome::Passed))};
    conjunction.expressions.push_back(observed("b", ObjectiveOutcome::Passed));
    conjunction.expressions.push_back({ioj::levels::AllExpression{.children = {0, 1}}});
    conjunction.roots[ioj::ObjectiveOutcome::Passed] = 2;
    auto disjunction{conjunction};
    disjunction.id = ioj::levels::ObjectiveName{"or"};
    disjunction.expressions[2] = {levels::AnyExpression{.children = {0, 1}}};
    auto compiled{compile_mission({.objectives = {a, b, conjunction, disjunction}}, {}, clock)};
    ASSERT_TRUE(compiled);
    auto const and_root{compiled->objectives.roots(ObjectiveId{2})[ioj::ObjectiveOutcome::Passed]};
    auto const or_root{compiled->objectives.roots(ObjectiveId{3})[ioj::ObjectiveOutcome::Passed]};
    mission = Mission{std::move(*compiled)};
    mission.begin({0, ledger, {}});
    tick(10);
    EXPECT_EQ(mission.result(and_root), ExprResult::False);
    EXPECT_EQ(mission.result(or_root), ExprResult::Pending);
    tick(20);
    EXPECT_EQ(mission.result(or_root), ExprResult::False);
    EXPECT_FALSE(mission.outcome(ioj::sim::ObjectiveId{2}));
    EXPECT_FALSE(mission.outcome(ioj::sim::ObjectiveId{3}));
}

TEST_F(MissionTest, ShortCircuitDoesNotDisableSharedLeaf) {
    auto source{objective("shared", timer(0))};
    source.expressions.push_back(timer(2));
    source.expressions.push_back({ioj::levels::AnyExpression{.children = {0, 1}}});
    source.expressions.push_back({ioj::levels::AllExpression{.children = {1, 2}}});
    source.roots[ioj::ObjectiveOutcome::Passed] = 3;
    start({.objectives = {source}});
    EXPECT_FALSE(mission.outcome(ioj::sim::ObjectiveId{0}));
    tick(20);
    EXPECT_EQ(mission.outcome(ioj::sim::ObjectiveId{0}), ObjectiveOutcome::Passed);
}

TEST_F(MissionTest, ActivationRelativeTimersAndOutcomeBranches) {
    auto first{objective("first", timer(1))};
    auto next{objective("next", timer(2))};
    next.dependencies.push_back({ioj::levels::ObjectiveName{"first"}, ObjectiveOutcome::Passed});
    auto other{objective("other", timer(0))};
    other.dependencies.push_back({ioj::levels::ObjectiveName{"first"}, ObjectiveOutcome::Failed});
    start({.objectives = {first, next, other}});
    EXPECT_EQ(mission.inactive_objectives().size(), 2);
    tick(10);
    EXPECT_EQ(mission.active_objectives().size(), 1);
    tick(29);
    EXPECT_FALSE(mission.outcome(ioj::sim::ObjectiveId{1}));
    tick(30);
    EXPECT_EQ(mission.outcome(ioj::sim::ObjectiveId{1}), ObjectiveOutcome::Passed);
    EXPECT_FALSE(mission.outcome(ioj::sim::ObjectiveId{2}));
    EXPECT_EQ(mission.inactive_objectives().size(), 1);
}

TEST_F(MissionTest, AllPrerequisitesAndLateOutcomeObservationSettleInSameTick) {
    auto first{objective("first", timer(1))};
    auto second{objective("second", timer(2), ObjectiveOutcome::Expired)};
    auto third{objective("third", observed("first", ObjectiveOutcome::Passed))};
    third.dependencies = {{ioj::levels::ObjectiveName{"first"}, ObjectiveOutcome::Passed},
                          {ioj::levels::ObjectiveName{"second"}, ObjectiveOutcome::Expired}};
    start({.objectives = {first, second, third}});
    tick(10);
    EXPECT_EQ(mission.inactive_objectives().size(), 1);
    tick(20);
    EXPECT_EQ(mission.outcome(ioj::sim::ObjectiveId{2}), ObjectiveOutcome::Passed);
    EXPECT_TRUE(mission.active_rules().empty());
}

TEST_F(MissionTest, BypassExpiresOnlyExplicitObservers) {
    auto bypass{objective("bypass", timer(1))};
    auto a{objective("a", observed("bypass", ObjectiveOutcome::Passed), ObjectiveOutcome::Expired)};
    auto b{a};
    b.id = ioj::levels::ObjectiveName{"b"};
    auto unrelated{objective("unrelated", timer(10))};
    start({.objectives = {a, b, bypass, unrelated}});
    tick(10);
    EXPECT_EQ(mission.outcome(ioj::sim::ObjectiveId{0}), ObjectiveOutcome::Expired);
    EXPECT_EQ(mission.outcome(ioj::sim::ObjectiveId{1}), ObjectiveOutcome::Expired);
    EXPECT_FALSE(mission.outcome(ioj::sim::ObjectiveId{3}));
    EXPECT_EQ(mission.active_objectives().size(), 1);
}

TEST_F(MissionTest, ReciprocalTeamsAndConsumeOnceCompletion) {
    auto attack{objective("attack", timer(1))};
    auto defend{objective(
        "defend", observed("attack", ObjectiveOutcome::Passed), ObjectiveOutcome::Failed)};
    defend.team = Team::Red;
    start({.objectives = {attack, defend},
           .team_results = {{Team::Blue,
                             {ioj::levels::ObjectiveName{"attack"}, ObjectiveOutcome::Passed},
                             TeamOutcome::Victory},
                            {Team::Red,
                             {ioj::levels::ObjectiveName{"defend"}, ObjectiveOutcome::Failed},
                             TeamOutcome::Defeat}}});
    tick(10);
    EXPECT_EQ(mission.team_outcome(Team::Blue), TeamOutcome::Victory);
    EXPECT_EQ(mission.team_outcome(Team::Red), TeamOutcome::Defeat);
    EXPECT_EQ(mission.take_completions().size(), 2);
    EXPECT_TRUE(mission.take_completions().empty());
    tick(20);
    EXPECT_TRUE(mission.take_completions().empty());
}

TEST_F(MissionTest, RootAndTeamConflictPrecedenceIsDeterministic) {
    auto source{objective("conflict", timer(0))};
    source.roots =
        ioj::levels::ObjectiveRoots{std::array<ioj::levels::ExpressionIndex, 3>{0, 0, 0}};
    start({.objectives = {source},
           .team_results = {{Team::Blue,
                             {ioj::levels::ObjectiveName{"conflict"}, ObjectiveOutcome::Passed},
                             TeamOutcome::Victory},
                            {Team::Blue,
                             {ioj::levels::ObjectiveName{"conflict"}, ObjectiveOutcome::Passed},
                             TeamOutcome::Defeat}}});
    EXPECT_EQ(mission.outcome(ioj::sim::ObjectiveId{0}), ObjectiveOutcome::Passed);
    EXPECT_EQ(mission.team_outcome(Team::Blue), TeamOutcome::Defeat);
}

TEST_F(MissionTest, RuleRunsSortAndRebuildAfterMembershipChanges) {
    names.emplace_back("future");
    entities.emplace_back();
    auto source{objective("rules", timer(1))};
    source.expressions.push_back({ioj::levels::EntityDestroyedExpression{.entity = names[0]}});
    source.expressions.push_back({ioj::levels::AllExpression{.children = {0, 1}}});
    source.roots[ioj::ObjectiveOutcome::Passed] = 2;
    start({.objectives = {source}});
    ASSERT_EQ(mission.rule_runs().size(), 2);
    EXPECT_EQ(mission.rule_runs()[0].type, ConditionType::EntityDestroyed);
    EXPECT_EQ(mission.rule_runs()[1].type, ConditionType::TimeElapsed);
    tick(10);
    ASSERT_EQ(mission.rule_runs().size(), 1);
    EXPECT_EQ(mission.rule_runs()[0].rules.count, 1);
    EXPECT_FALSE(mission.outcome(ioj::sim::ObjectiveId{0}));
}

TEST_F(MissionTest, RejectsStructuralAndActivationCyclesButAllowsObservationCycles) {
    auto a{objective("a", observed("b", ObjectiveOutcome::Passed))};
    auto b{objective("b", observed("a", ObjectiveOutcome::Passed))};
    EXPECT_TRUE(compile_mission({.objectives = {a, b}}, {}, clock));
    a.dependencies = {{ioj::levels::ObjectiveName{"b"}, ObjectiveOutcome::Passed}};
    b.dependencies = {{ioj::levels::ObjectiveName{"a"}, ObjectiveOutcome::Passed}};
    EXPECT_FALSE(compile_mission({.objectives = {a, b}}, {}, clock));
    a.dependencies.clear();
    a.expressions = {{ioj::levels::AllExpression{.children = {1}}},
                     {ioj::levels::AnyExpression{.children = {0}}}};
    EXPECT_FALSE(compile_mission({.objectives = {a}}, {}, clock));
    std::get<levels::AnyExpression>(a.expressions[1].payload).children = {99};
    EXPECT_FALSE(compile_mission({.objectives = {a}}, {}, clock));
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
            (objective :id 'next :team 'blue
              :depends-on (list (objective-resolved 'first 'passed))
              :pass (all-of (objective-resolved 'first 'passed) (time-elapsed 2))))
          :team-results (list
            (team-result :team 'blue :when (objective-resolved 'next 'passed) :result 'victory)))))")};
    ASSERT_TRUE(parsed);
    ASSERT_TRUE(parsed->mission());
    names.emplace_back("player");
    entities.emplace_back();
    start(*parsed->mission());
    tick(10);
    EXPECT_EQ(mission.outcome(ioj::sim::ObjectiveId{0}), ObjectiveOutcome::Passed);
    EXPECT_FALSE(mission.team_outcome(Team::Blue));
    tick(29);
    EXPECT_FALSE(mission.outcome(ioj::sim::ObjectiveId{1}));
    tick(30);
    EXPECT_EQ(mission.team_outcome(Team::Blue), TeamOutcome::Victory);
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
                         (time-elapsed 1) (time-elapsed 10))
          :team-results (list
            (team-result :team 'blue :when (objective-resolved 'attack 'passed) :result 'victory)
            (team-result :team 'red :when (objective-resolved 'defend 'failed) :result 'defeat)))))")};
    ASSERT_TRUE(parsed);
    ASSERT_TRUE(parsed->mission());
    start(*parsed->mission());
    tick(10);
    EXPECT_EQ(mission.outcome(ioj::sim::ObjectiveId{0}), ObjectiveOutcome::Passed);
    EXPECT_EQ(mission.outcome(ioj::sim::ObjectiveId{1}), ObjectiveOutcome::Failed);
    EXPECT_EQ(mission.team_outcome(Team::Blue), TeamOutcome::Victory);
    EXPECT_EQ(mission.team_outcome(Team::Red), TeamOutcome::Defeat);
}

TEST_F(MissionTest, RejectsInvalidReferencesParametersAndDuplicatePrerequisites) {
    auto a{objective("a", timer(0))};
    auto b{objective("b", timer(1))};
    b.dependencies = {{ioj::levels::ObjectiveName{"a"}, ObjectiveOutcome::Passed},
                      {ioj::levels::ObjectiveName{"a"}, ObjectiveOutcome::Passed}};
    EXPECT_FALSE(compile_mission({.objectives = {a, b}}, {}, clock));
    EXPECT_FALSE(compile_mission({.objectives = {a, a}}, {}, clock));
    b.dependencies = {{ioj::levels::ObjectiveName{"missing"}, ObjectiveOutcome::Passed}};
    EXPECT_FALSE(compile_mission({.objectives = {a, b}}, {}, clock));
    EXPECT_FALSE(compile_mission(
        {.objectives = {a},
         .team_results = {{Team::Blue,
                           {ioj::levels::ObjectiveName{"missing"}, ObjectiveOutcome::Passed},
                           TeamOutcome::Victory}}},
        {},
        clock));
    a.expressions[0] = {ioj::levels::EntityReachedPositionExpression{
        .entity = levels::EntityId{"target"},
        .radius = ioj::NonNegativeFinite<double>::make(std::numeric_limits<double>::max())}};
    names.emplace_back("target");
    EXPECT_FALSE(compile_mission({.objectives = {a}}, names, clock));
}
}
