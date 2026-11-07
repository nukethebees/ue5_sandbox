#include "test_batch_orchestrator_setup_scenario.h"
#include <ioj/sim/telemetry/level_telemetry_manager.h>
#include <SandboxTests/support/simulation_step.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestBatchOrchestratorTestAccess.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <Misc/AutomationTest.h>
#include <SandboxISMCComponent.h>

namespace ioj {
FTestBatchOrchestratorSetupScenario::FTestBatchOrchestratorSetupScenario(
    FSimulationTestContext& context, EOrchestratorSetupScenario const scenario)
    : FSimulationTestScenario{context}
    , scenario_{scenario} {}

void FTestBatchOrchestratorSetupScenario::run() {
    switch (scenario_) {
        case EOrchestratorSetupScenario::SpawnMissingActors:
            prepare_level();
            break;
        case EOrchestratorSetupScenario::PresentationFrameOrdering:
            presentation_frame_ordering();
            break;
    }
}

void FTestBatchOrchestratorSetupScenario::prepare_level() {
    TestCommandBuilder.Do([this] {
        auto* const orchestrator{&context_.orchestrator};
        if (!TestRunner->TestNotNull(TEXT("Orchestrator is available"), orchestrator)) {
            return;
        }

        if (!load_player_test_level(*orchestrator, checks)) {
            return;
        }

        TestRunner->TestTrue(TEXT("Paused test start mode defers orchestrator initialisation"),
                             orchestrator->get_state() == sim::LevelState::Uninitialised);
        TestRunner->TestFalse(TEXT("Uninitialised orchestrator does not tick"),
                              orchestrator->IsActorTickEnabled());

        auto const resources{orchestrator->get_presentation_resources()};
        TestRunner->TestTrue(TEXT("Batch components are available"), resources.is_valid());
        TestRunner->TestTrue(TEXT("Orchestrator owns the laser batch component"),
                             resources.lasers->GetOwner() == orchestrator);
        TestRunner->TestTrue(TEXT("Orchestrator owns the fighter batch component"),
                             resources.fighters->GetOwner() == orchestrator);
        for (auto* component : {resources.capital_ships, resources.turrets, resources.spinners}) {
            TestRunner->TestTrue(TEXT("Orchestrator owns each batch component"),
                                 component->GetOwner() == orchestrator);
        }
        TestRunner->TestNull(TEXT("Simulation construction is deferred"),
                             orchestrator->get_level_simulation());
        initialise_test_driver();

        orchestrator->start_simulation();

        TestRunner->TestTrue(TEXT("Starting transitions the orchestrator to running"),
                             orchestrator->get_state() == sim::LevelState::Running);
        TestRunner->TestTrue(TEXT("Running orchestrator ticks"),
                             orchestrator->IsActorTickEnabled());
        TestRunner->TestEqual(
            TEXT("Starting does not immediately advance simulation"),
            orchestrator->get_level_simulation()->clock().get_completed_ticks().value(),
            uint64{0});
    });
}

void FTestBatchOrchestratorSetupScenario::presentation_frame_ordering() {
    TestCommandBuilder.Do([this] {
        auto& driver{initialise_test_driver()};
        auto& orchestrator{driver.orchestrator};
        FTestBatchOrchestratorTestAccess::use_settings_file(orchestrator,
                                                            TEXT("LevelScripts/Settings/base.scm"));
        orchestrator.set_presentation_enabled(true);
        if (!load_player_test_level(orchestrator, checks)) {
            return;
        }
        orchestrator.start_simulation();
        auto const* presentation{orchestrator.get_level_presentation()};
        if (!checks.is_true(presentation != nullptr,
                            TEXT("Orchestrator constructs presentation"))) {
            SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
            return;
        }
        FTestBatchOrchestratorTestAccess::set_telemetry_metadata(
            orchestrator, sim::telemetry::RunMetadata{.run_id = "frame-ordering"});
        struct FFrameObservation {
            uint64 completed_ticks{};
            uint64 presented_tick{};
        };
        TimeSeriesData<FFrameObservation> observations;
        auto const observe{[&observations](ATestBatchOrchestrator& owner) {
            auto const& clock{owner.get_level_simulation()->clock()};
            observations.add(
                clock.simulation_time(),
                FFrameObservation{clock.get_completed_ticks().value(),
                                  owner.get_level_presentation()->get_last_completed_tick()});
        }};
        auto const dt{orchestrator.get_level_simulation()->clock().tick_period()};
        advance_simulation(orchestrator, 0.0, observe);
        checks.are_equal(uint64{0},
                         presentation->get_last_completed_tick(),
                         TEXT("Zero fixed ticks preserve the presented state"));
        advance_simulation(orchestrator, dt * 4.25, observe);
        checks.are_equal(uint64{4},
                         presentation->get_last_completed_tick(),
                         TEXT("Presentation observes the final fixed tick"));
        orchestrator.pause_simulation();
        advance_simulation(orchestrator, dt, observe);
        checks.are_equal(uint64{4},
                         presentation->get_last_completed_tick(),
                         TEXT("Paused frames freeze presentation"));
        orchestrator.start_simulation();
        advance_simulation(orchestrator, dt * 4.0, observe);
        FTestBatchOrchestratorTestAccess::stop_run(orchestrator,
                                                   sim::telemetry::RunEndReason::DurationReached);
        checks.are_equal(
            uint32{2}, observations.num(), TEXT("Each catch-up frame produces one observation"));
        auto const sample_count{observations.num()};
        for (uint32 index{}; index < sample_count; ++index) {
            auto const& sample{observations.value_at(index)};
            checks.are_equal(static_cast<uint64>((index + 1) * 4),
                             sample.completed_ticks,
                             TEXT("Catch-up frames observe their final completed ticks"));
            checks.are_equal(sample.completed_ticks,
                             sample.presented_tick,
                             TEXT("Observations follow the frame presentation"));
        }
        checks.are_equal(uint64{8},
                         presentation->get_last_completed_tick(),
                         TEXT("Terminal presentation observes the final completed tick"));
        auto report{
            FTestBatchOrchestratorTestAccess::take_finalized_telemetry_report(orchestrator)};
        if (checks.is_true(report.IsSet(), TEXT("Terminal frame produces a telemetry report"))) {
            checks.are_equal(uint64{8},
                             report->run.completion.completed_ticks.value(),
                             TEXT("Telemetry retains the final resolved tick"));
            checks.is_true(!report->run.battle_samples.empty() &&
                               report->run.battle_samples.back().completed_tick.value() == 8,
                           TEXT("Terminal battle sample observes the final resolved tick"));
        }
        checks.is_true(
            !FTestBatchOrchestratorTestAccess::take_finalized_telemetry_report(orchestrator)
                 .IsSet(),
            TEXT("Finalized report is consumed only once"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    });
}

}
