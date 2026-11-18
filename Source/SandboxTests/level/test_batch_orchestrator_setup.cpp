#include "test_batch_orchestrator_setup_scenario.h"
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

        TestRunner->TestTrue(TEXT("Loading constructs a paused simulation"),
                             orchestrator->get_state() == sim::LevelState::Paused);
        TestRunner->TestTrue(TEXT("Loaded orchestrator ticks to process control requests"),
                             orchestrator->IsActorTickEnabled());

        auto const resources{
            FTestBatchOrchestratorTestAccess::presentation_resources(*orchestrator)};
        TestRunner->TestTrue(TEXT("Batch components are available"), resources.is_valid());
        TestRunner->TestTrue(TEXT("Orchestrator owns the laser batch component"),
                             resources.lasers->GetOwner() == orchestrator);
        TestRunner->TestTrue(TEXT("Orchestrator owns the fighter batch component"),
                             resources.fighters->GetOwner() == orchestrator);
        for (auto* component : {resources.capital_ships, resources.turrets, resources.spinners}) {
            TestRunner->TestTrue(TEXT("Orchestrator owns each batch component"),
                                 component->GetOwner() == orchestrator);
        }
        if (!TestRunner->TestNotNull(TEXT("Loading constructs the simulation"),
                                     orchestrator->get_level_simulation())) {
            return;
        }
        TestRunner->TestNotNull(
            TEXT("Loading initializes presentation"),
            FTestBatchOrchestratorTestAccess::level_presentation(*orchestrator));
        orchestrator->tick(1.0);
        TestRunner->TestEqual(
            TEXT("Loaded simulation stays at tick zero until started"),
            orchestrator->get_level_simulation()->clock().get_completed_ticks().value(),
            uint64{0});
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
        orchestrator.set_presentation_enabled(true);
        if (!load_player_test_level(orchestrator, checks)) {
            return;
        }
        orchestrator.start_simulation();
        auto const* presentation{
            FTestBatchOrchestratorTestAccess::level_presentation(orchestrator)};
        if (!checks.is_true(presentation != nullptr,
                            TEXT("Orchestrator constructs presentation"))) {
            SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
            return;
        }
        uint32 observation_count{};
        auto const observe{[this, &observation_count](ATestBatchOrchestrator& owner) {
            ++observation_count;
            auto const completed_ticks{
                owner.get_level_simulation()->clock().get_completed_ticks().value()};
            checks.are_equal(static_cast<uint64>(observation_count * 4),
                             completed_ticks,
                             TEXT("Catch-up frames observe their final completed ticks"));
            checks.are_equal(completed_ticks,
                             FTestBatchOrchestratorTestAccess::level_presentation(owner)
                                 ->get_last_completed_tick(),
                             TEXT("Observations follow the frame presentation"));
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
                                                   sim::RunEndReason::DurationReached);
        checks.are_equal(
            uint32{2}, observation_count, TEXT("Each catch-up frame produces one observation"));
        checks.are_equal(uint64{8},
                         presentation->get_last_completed_tick(),
                         TEXT("Terminal presentation observes the final completed tick"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    });
}

}
