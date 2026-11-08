#include <ioj/sim/telemetry/run_catalog.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestBatchOrchestratorTestAccess.h>
#include <SpaceGame/persistence/SpaceSaveSubsystem.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/telemetry/LevelTelemetryJson.h>

#include <CoreGlobals.h>
#include <CQTest.h>
#include <Misc/ScopeExit.h>

#include <algorithm>
#include <filesystem>

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

    TEST_METHOD(InitialAndTickDrivenOutcomesAreReportedOnce)
    {
        TestCommandBuilder.Do([this] {
            auto& orchestrator{*level_setup.get_orchestrator()};
            auto& profiles{
                ioj::USpaceSaveSubsystem::get(orchestrator.GetGameInstance())->profiles()};
            for (auto const initial : {true, false}) {
                for (auto const winner : {ioj::Team::Blue, ioj::Team::Red}) {
                    auto const before{profiles.active_records().size()};
                    TArray<FLevelCompletion> completions;
                    auto const listener{orchestrator.on_level_completed.AddLambda(
                        [&](FLevelCompletion const& completion) {
                            completions.Add(completion);
                            orchestrator.tick(0.0);
                        })};
                    ON_SCOPE_EXIT {
                        orchestrator.on_level_completed.Remove(listener);
                    };

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
                    auto const* simulation{orchestrator.get_level_simulation()};
                    checks.is_true(simulation->result()->winner == winner,
                                   TEXT("First terminal result retains the winning team"));
                    checks.are_equal(initial ? uint64{0} : uint64{1},
                                     simulation->result()->terminal.tick.value(),
                                     TEXT("Completion retains the actual terminal tick"));
                    orchestrator.tick(0.0);
                    orchestrator.tick(1.0);
                    checks.are_equal(1, completions.Num(), TEXT("Completion callback occurs once"));
                    checks.are_equal(before + 1,
                                     profiles.active_records().size(),
                                     TEXT("Completion appends one profile record"));
                    if (completions.Num() == 1) {
                        checks.is_true(completions[0].state == (winner == ioj::Team::Blue
                                                                    ? ioj::ELevelOutcome::Victory
                                                                    : ioj::ELevelOutcome::Defeat),
                                       TEXT("Callback distinguishes victory and defeat"));
                        checks.are_equal(FString{TEXT("level_completed")},
                                         completions[0].end_reason,
                                         TEXT("Callback retains the termination reason"));
                    }
                    auto first{simulation->telemetry_run()};
                    auto second{simulation->telemetry_run()};
                    checks.is_true(first && second &&
                                       first->completion.completed_ticks ==
                                           second->completion.completed_ticks,
                                   TEXT("Telemetry stays queryable after completion handling"));
                    orchestrator.reset_for_new_level();
                    checks.are_equal(before + 1,
                                     profiles.active_records().size(),
                                     TEXT("Teardown does not append another profile record"));
                    checks.are_equal(
                        1, completions.Num(), TEXT("Teardown does not replay the callback"));
                }
            }
        });
    }

    TEST_METHOD(ExternalStopWithoutAnotherTickIsReportedOnce)
    {
        TestCommandBuilder.Do([this] {
            auto& orchestrator{*level_setup.get_orchestrator()};
            if (!load()) {
                return;
            }
            auto& profiles{
                ioj::USpaceSaveSubsystem::get(orchestrator.GetGameInstance())->profiles()};
            auto const before{profiles.active_records().size()};
            TArray<FLevelCompletion> completions;
            auto const listener{orchestrator.on_level_completed.AddLambda(
                [&](FLevelCompletion const& completion) { completions.Add(completion); })};
            ON_SCOPE_EXIT {
                orchestrator.on_level_completed.Remove(listener);
            };
            FTestBatchOrchestratorTestAccess::stop_run(
                orchestrator, ioj::sim::telemetry::RunEndReason::DurationReached);
            orchestrator.tick(0.0);
            orchestrator.tick(0.0);
            checks.are_equal(
                uint64{0},
                orchestrator.get_level_simulation()->clock().get_completed_ticks().value(),
                TEXT("External completion needs no simulation tick"));
            checks.are_equal(1, completions.Num(), TEXT("External stop is reported once"));
            if (completions.Num() == 1) {
                checks.is_true(completions[0].state == ioj::ELevelOutcome::Stopped,
                               TEXT("Stopping does not invent a winner"));
                checks.are_equal(FString{TEXT("duration_reached")},
                                 completions[0].end_reason,
                                 TEXT("External stop retains its reason"));
            }
            checks.are_equal(before + 1,
                             profiles.active_records().size(),
                             TEXT("External stop appends one profile record"));
        });
    }

    TEST_METHOD(TeardownSavesPendingResultsWithoutOpeningUi)
    {
        TestCommandBuilder.Do([this] {
            auto& orchestrator{*level_setup.get_orchestrator()};
            if (!load()) {
                return;
            }
            auto& profiles{
                ioj::USpaceSaveSubsystem::get(orchestrator.GetGameInstance())->profiles()};
            auto const before{profiles.active_records().size()};
            int32 notifications{};
            auto const listener{orchestrator.on_level_completed.AddLambda(
                [&](FLevelCompletion const&) { ++notifications; })};
            ON_SCOPE_EXIT {
                orchestrator.on_level_completed.Remove(listener);
            };
            FTestBatchOrchestratorTestAccess::stop_run(
                orchestrator, ioj::sim::telemetry::RunEndReason::DurationReached);
            auto const run_id{
                orchestrator.get_level_simulation()->telemetry_run()->metadata.run_id};
            {
                TGuardValue<bool> persist_reports{GIsAutomationTesting, false};
                orchestrator.reset_for_new_level();
            }
            checks.is_true(orchestrator.get_level_simulation() == nullptr,
                           TEXT("Teardown releases the simulation"));
            checks.are_equal(0, notifications, TEXT("Teardown does not open completion UI"));
            checks.are_equal(before + 1,
                             profiles.active_records().size(),
                             TEXT("Teardown records the pending result"));

            ioj::sim::telemetry::RunCatalog catalog{
                std::filesystem::path{*level_telemetry_runs_directory()}};
            catalog.refresh();
            auto const saved{std::ranges::find(
                catalog.runs(), run_id, &ioj::sim::telemetry::RunSummary::run_id)};
            if (!checks.is_true(saved != catalog.runs().end(),
                                TEXT("Teardown persists telemetry"))) {
                return;
            }
            ON_SCOPE_EXIT {
                std::filesystem::remove(saved->path);
            };
            checks.is_true(catalog.select_run(run_id),
                           TEXT("Saved telemetry reloads after teardown"));
            if (auto const* report{catalog.selected_record()}) {
                checks.is_true(report->run.completion.reason ==
                                   ioj::sim::telemetry::RunEndReason::DurationReached,
                               TEXT("Teardown preserves the original completion reason"));
                checks.are_equal(uint64{0},
                                 report->run.completion.completed_ticks.value(),
                                 TEXT("Saved report preserves the terminal tick"));
            }
            if (!load()) {
                return;
            }
            orchestrator.reset_for_new_level();
            checks.are_equal(before + 1,
                             profiles.active_records().size(),
                             TEXT("Reset-only stops do not create profile results"));
            checks.are_equal(
                0, notifications, TEXT("Reset-only stops do not notify completion UI"));
        });
    }
};
