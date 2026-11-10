#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestBatchOrchestratorTestAccess.h>
#include <SpaceGame/persistence/SpaceSaveSubsystem.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <CQTest.h>

TEST_CLASS(TerminalCompletion, "Sandbox.LevelTests")
{
    inline static ioj::FTestBatchOrchestratorLevelSetup level_setup{};
    ioj::FSoftTestAssertions checks{};

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        level_setup.begin_test(TestCommandBuilder, *TestRunner, checks);
    }

    AFTER_EACH()
    { level_setup.end_test(); }

    AFTER_ALL()
    { level_setup.teardown(); }

    auto load(ioj::levels::LogicDefinition logic = {}) -> bool {
        auto definition{ioj::levels::LevelDefinition::make({
            .metadata = {.id = ioj::levels::LevelId{"terminal-test"}, .title = "Terminal test"},
            .player_entity_id = ioj::levels::EntityId{"player"},
            .logic = std::move(logic),
            .entities = {{.id = ioj::levels::EntityId{"player"},
                          .archetype = ioj::levels::EntityArchetype::PlayerFighter,
                          .team = ioj::Team::Blue}},
        })};
        if (!checks.is_true(definition.has_value(), TEXT("Terminal test definition is valid"))) {
            return false;
        }
        auto const loaded{level_setup.get_orchestrator()->load_level(*definition)};
        return checks.is_true(loaded.has_value(),
                              loaded ? TEXT("Terminal test level loads")
                                     : loaded.error().format(TEXT("terminal-test")));
    }

    TEST_METHOD(InitialAndTickDrivenOutcomesAreSavedOnce)
    {
        TestCommandBuilder.Do([this] {
            auto& orchestrator{*level_setup.get_orchestrator()};
            auto& profiles{
                ioj::USpaceSaveSubsystem::get(orchestrator.GetGameInstance())->profiles()};
            for (auto const initial : {true, false}) {
                for (auto const winner : {ioj::Team::Blue, ioj::Team::Red}) {
                    auto const before{profiles.active_records().size()};
                    ioj::levels::LogicDefinition logic;
                    if (initial) {
                        logic.initial_effects = {ioj::levels::WinEffect{winner}};
                    } else {
                        logic.expressions = {{ioj::levels::TimerElapsedExpr{0}}};
                        logic.timer_count = 1;
                        logic.initial_effects = {ioj::levels::StartTimerEffect{0, {}}};
                        logic.rules = {{0, {ioj::levels::WinEffect{winner}}}};
                    }
                    if (!load(std::move(logic))) {
                        return;
                    }
                    if (!initial) {
                        orchestrator.start_simulation();
                        orchestrator.tick(
                            orchestrator.get_level_simulation()->clock().tick_period());
                    }
                    checks.is_true(orchestrator.get_level_simulation() == nullptr,
                                   TEXT("Ending the level releases the simulation"));
                    orchestrator.tick(0.0);
                    orchestrator.tick(1.0);
                    if (!checks.are_equal(before + 1,
                                          profiles.active_records().size(),
                                          TEXT("Completion saves one profile record"))) {
                        return;
                    }
                    auto const& record{profiles.active_records().back()};
                    checks.is_true(record.outcome == (winner == ioj::Team::Blue
                                                          ? ioj::LevelOutcome::Victory
                                                          : ioj::LevelOutcome::Defeat),
                                   TEXT("Saved score distinguishes victory and defeat"));
                    checks.is_true(record.end_reason == ioj::sim::RunEndReason::LevelCompleted,
                                   TEXT("Saved score retains the termination reason"));
                    checks.is_true(initial ? record.time_seconds == 0.f : record.time_seconds > 0.f,
                                   TEXT("Saved time reflects initialization or tick completion"));
                    orchestrator.reset_for_new_level();
                    checks.are_equal(before + 1,
                                     profiles.active_records().size(),
                                     TEXT("Reset does not save the same result again"));
                }
            }
        });
    }

    TEST_METHOD(ExternalStopWithoutAnotherTickIsSavedOnce)
    {
        TestCommandBuilder.Do([this] {
            auto& orchestrator{*level_setup.get_orchestrator()};
            if (!load()) {
                return;
            }
            auto& profiles{
                ioj::USpaceSaveSubsystem::get(orchestrator.GetGameInstance())->profiles()};
            auto const before{profiles.active_records().size()};
            FTestBatchOrchestratorTestAccess::stop_run(orchestrator,
                                                       ioj::sim::RunEndReason::DurationReached);
            orchestrator.tick(0.0);
            orchestrator.tick(0.0);
            checks.is_true(orchestrator.get_level_simulation() == nullptr,
                           TEXT("External stop releases the simulation"));
            if (!checks.are_equal(before + 1,
                                  profiles.active_records().size(),
                                  TEXT("External stop saves one profile record"))) {
                return;
            }
            auto const& record{profiles.active_records().back()};
            checks.are_equal(0.f, record.time_seconds, TEXT("External stop needs no tick"));
            checks.is_true(record.outcome == ioj::LevelOutcome::Stopped,
                           TEXT("Stopping does not invent a winner"));
            checks.is_true(record.end_reason == ioj::sim::RunEndReason::DurationReached,
                           TEXT("External stop retains its reason"));
        });
    }

    TEST_METHOD(TeardownSavesPendingResults)
    {
        TestCommandBuilder.Do([this] {
            auto& orchestrator{*level_setup.get_orchestrator()};
            if (!load()) {
                return;
            }
            auto& profiles{
                ioj::USpaceSaveSubsystem::get(orchestrator.GetGameInstance())->profiles()};
            auto const before{profiles.active_records().size()};
            FTestBatchOrchestratorTestAccess::stop_run(orchestrator,
                                                       ioj::sim::RunEndReason::DurationReached);
            orchestrator.reset_for_new_level();
            checks.is_true(orchestrator.get_level_simulation() == nullptr,
                           TEXT("Teardown releases the simulation"));
            if (!checks.are_equal(before + 1,
                                  profiles.active_records().size(),
                                  TEXT("Teardown saves the pending result"))) {
                return;
            }
            checks.is_true(profiles.active_records().back().end_reason ==
                               ioj::sim::RunEndReason::DurationReached,
                           TEXT("Teardown preserves the pending completion reason"));
            if (!load()) {
                return;
            }
            orchestrator.reset_for_new_level();
            checks.are_equal(before + 1,
                             profiles.active_records().size(),
                             TEXT("Reset-only stops do not create profile results"));
        });
    }
};
