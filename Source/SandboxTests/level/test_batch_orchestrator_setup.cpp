#include <SandboxTests/support/test_setup.h>
#include "test_batch_orchestrator_setup_scenario.h"

#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/TestActorSpawning.h>

#include <SpaceGame/simulation/LevelTelemetryManager.h>
#include <SpaceGame/simulation/SimulationClockInterface.h>
#include <SpaceGame/simulation/SimulationConfig.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFighters.h>
#include <SpaceGame/ships/capital/TestCapitalShips.h>
#include <SpaceGame/combat/lasers/TestLasers.h>
#include <SpaceGame/missions/TestMissionManager.h>
#include <SpaceGame/simulation/TestSimulationConfig.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/defences/turrets/TestStaticTurrets.h>
#include <SpaceGame/defences/spinners/TestTubeSpinners.h>
#include <SpaceGame/effects/DelayedNiagaraSpawner.h>

#include <SandboxCoreEngine/actor_utils.h>

#include <Misc/AutomationTest.h>

namespace ml {
FTestBatchOrchestratorSetupScenario::FTestBatchOrchestratorSetupScenario(
    FSimulationTestContext& context, EOrchestratorSetupScenario const scenario)
    : FSimulationTestScenario{context}
    , scenario_{scenario} {}

void FTestBatchOrchestratorSetupScenario::run() {
    switch (scenario_) {
        case EOrchestratorSetupScenario::SpawnMissingActors:
            spawn_missing_actors();
            break;
        case EOrchestratorSetupScenario::SimulationClockConversions:
            simulation_clock_conversions();
            break;
        case EOrchestratorSetupScenario::LevelTelemetry:
            level_telemetry();
            break;
    }
}

void FTestBatchOrchestratorSetupScenario::tear_down() {
    if (test_driver.IsSet()) {
        test_driver->orchestrator.clear_end_tick_test_hook();
        test_driver->orchestrator.pause_simulation();
    }
}

void FTestBatchOrchestratorSetupScenario::spawn_missing_actors() {
    TestCommandBuilder.Do([this] {
        auto* const orchestrator{&context_.orchestrator};
        if (!TestRunner->TestNotNull(TEXT("Orchestrator is available"), orchestrator)) {
            return;
        }

        auto& world{context_.world};

        TestRunner->TestTrue(TEXT("Paused test start mode defers orchestrator initialisation"),
                             orchestrator->get_state() == EOrchestratorState::Uninitialised);
        TestRunner->TestFalse(TEXT("Uninitialised orchestrator does not tick"),
                              orchestrator->IsActorTickEnabled());

        TestRunner->TestEqual(
            TEXT("One lasers actor exists"), ml::count_actors<ATestLasers>(world), 1);
        TestRunner->TestEqual(
            TEXT("One capital ships actor exists"), ml::count_actors<ATestCapitalShips>(world), 1);
        TestRunner->TestEqual(TEXT("One fighters actor exists"),
                              ml::count_actors<ATestCapitalShipFighters>(world),
                              1);
        TestRunner->TestEqual(
            TEXT("One turrets actor exists"), ml::count_actors<ATestStaticTurrets>(world), 1);
        TestRunner->TestEqual(
            TEXT("One spinners actor exists"), ml::count_actors<ATestTubeSpinners>(world), 1);
        TestRunner->TestEqual(
            TEXT("One Niagara spawner exists"), ml::count_actors<ADelayedNiagaraSpawner>(world), 1);

        TestRunner->TestNotNull(TEXT("Lasers are bound"), orchestrator->get_lasers());
        TestRunner->TestNotNull(TEXT("Capital ships are bound"), orchestrator->get_capital_ships());
        TestRunner->TestNotNull(TEXT("Fighters are bound"),
                                orchestrator->get_capital_ship_fighters());
        TestRunner->TestNotNull(TEXT("Turrets are bound"), orchestrator->get_turrets());
        TestRunner->TestNotNull(TEXT("Spinners are bound"), orchestrator->get_spinners());
        TestRunner->TestNotNull(TEXT("Entity registry is embedded"),
                                &orchestrator->get_entity_registry());
        TestRunner->TestNotNull(TEXT("Mission manager is embedded"),
                                &orchestrator->get_mission_manager());
        TestRunner->TestNotNull(TEXT("Niagara spawner is bound"),
                                orchestrator->get_niagara_spawner());

        auto const completed_ticks{orchestrator->get_completed_ticks()};
        orchestrator->start_simulation();

        TestRunner->TestTrue(TEXT("Starting transitions the orchestrator to running"),
                             orchestrator->get_state() == EOrchestratorState::Running);
        TestRunner->TestTrue(TEXT("Running orchestrator ticks"),
                             orchestrator->IsActorTickEnabled());
        TestRunner->TestEqual(TEXT("Starting does not immediately advance simulation"),
                              orchestrator->get_completed_ticks(),
                              completed_ticks);
    });
}

void FTestBatchOrchestratorSetupScenario::simulation_clock_conversions() {
    TestCommandBuilder.Do([this] {
        auto* const orchestrator{&context_.orchestrator};
        if (!TestRunner->TestNotNull(TEXT("Orchestrator is available"), orchestrator)) {
            return;
        }

        TestRunner->TestEqual(
            TEXT("Reusable level is constructed once"), context_.level_construction_count, 1);
        TestRunner->TestTrue(TEXT("Reset leaves the orchestrator uninitialised"),
                             orchestrator->get_state() == EOrchestratorState::Uninitialised);

        ml::test_batch_orchestrator::SimulationClockInterface clock;
        clock.bind(*orchestrator);

        TestRunner->TestEqual(TEXT("Tick-rate frequency has a one-tick period"),
                              clock.frequency_to_tick_period(60.0),
                              uint64{1});
        TestRunner->TestEqual(
            TEXT("Frequency periods round up"), clock.frequency_to_tick_period(24.0), uint64{3});
        TestRunner->TestEqual(TEXT("Above-tick-rate frequency has a one-tick period"),
                              clock.frequency_to_tick_period(120.0),
                              uint64{1});
        TestRunner->TestEqual(TEXT("Zero duration has a zero-tick period"),
                              clock.duration_to_tick_period(0.0),
                              uint64{0});
        TestRunner->TestEqual(TEXT("One second uses the configured tick rate"),
                              clock.duration_to_tick_period(1.0),
                              uint64{60});
        TestRunner->TestEqual(
            TEXT("Duration periods round up"), clock.duration_to_tick_period(0.025), uint64{2});
    });
}

void FTestBatchOrchestratorSetupScenario::level_telemetry() {
    TestCommandBuilder.Do([this] { begin_level_telemetry(); })
        .Until(
            [this] {
                return !checks.all_passed ||
                       (test_driver.IsSet() && test_driver->timeline.is_finished());
            },
            FTimespan{0, 0, 1})
        .Then([this] { check_level_telemetry(); });
}

void FTestBatchOrchestratorSetupScenario::begin_level_telemetry() {
    auto const* const simulation_config{context_.config.simulation_config.Get()};
    if (!checks.not_nullptr(simulation_config, TEXT("Simulation config is available"))) {
        return;
    }
    auto* const player{spawn_player_ship(context_.world,
                                         context_.config.actor_classes.player_ship_class,
                                         simulation_config->player_ship_config)};
    if (!checks.is_valid(player, TEXT("Telemetry player ship is spawned"))) {
        return;
    }
    context_.orchestrator.set_player_ship(*player);

    test_driver = TestSimulationDriver::from_world(context_.world);
    telemetry_observations.reset();
    telemetry_observations.reserve(16);

    test_driver->orchestrator.start_simulation();
    initial_active_entity_count = test_driver->registry.get_num_alive_active_entities();
    test_driver->orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateRaw(
        this, &FTestBatchOrchestratorSetupScenario::on_level_telemetry_end_tick));
    test_driver->timeline.then_after(0.05, [this] { kill_telemetry_test_entity(); });
    test_driver->timeline.finish_at(0.15);
}

void FTestBatchOrchestratorSetupScenario::kill_telemetry_test_entity() {
    auto const& telemetry{
        test_driver->orchestrator.get_level_telemetry_manager().get_active_entity_count_data()};
    telemetry_samples_before_change = telemetry.num();

    TStaticArray<FRegistryEntityHandle, 1> const targets{
        test_driver->get_player_ship().get_entity_handle()};
    test_driver->queue_kills(targets);
}

void FTestBatchOrchestratorSetupScenario::on_level_telemetry_end_tick(
    ATestBatchOrchestrator& orchestrator) {
    auto const& telemetry{
        orchestrator.get_level_telemetry_manager().get_active_entity_count_data()};
    check(!telemetry.is_empty());

    telemetry_observations.add(
        test_driver->get_time(),
        FTelemetryObservation{
            .completed_ticks = orchestrator.get_completed_ticks(),
            .telemetry_sample_count = telemetry.num(),
            .last_telemetry_tick = telemetry.last_time(),
            .telemetry_entity_count = telemetry.last_value(),
            .registry_entity_count = test_driver->registry.get_num_alive_active_entities(),
        });
    test_driver->timeline.tick(test_driver->get_time());
}

void FTestBatchOrchestratorSetupScenario::check_level_telemetry() {
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    checks.is_greater_than(
        telemetry_observations.num(), int32{0}, TEXT("Telemetry observations recorded"));
    if (telemetry_observations.is_empty()) {
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
        return;
    }

    auto const& initial_observation{telemetry_observations.value_at(0)};
    checks.are_equal(
        int32{1}, initial_observation.telemetry_sample_count, TEXT("Tick-zero baseline recorded"));
    checks.are_equal(
        uint64{0}, initial_observation.last_telemetry_tick, TEXT("Baseline uses tick zero"));
    checks.are_equal(initial_active_entity_count,
                     initial_observation.telemetry_entity_count,
                     TEXT("Baseline records initial active entities"));

    int32 changed_observation_index{INDEX_NONE};
    auto const observation_count{telemetry_observations.num()};
    for (int32 i{0}; i < observation_count; ++i) {
        if (telemetry_observations.value_at(i).telemetry_sample_count >
            telemetry_samples_before_change) {
            changed_observation_index = i;
            break;
        }
    }
    checks.is_true(changed_observation_index != INDEX_NONE,
                   TEXT("Changed active entity count is sampled"));
    if (changed_observation_index == INDEX_NONE) {
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
        return;
    }

    auto const& changed_observation{telemetry_observations.value_at(changed_observation_index)};
    checks.are_equal(changed_observation.completed_ticks,
                     changed_observation.last_telemetry_tick,
                     TEXT("Telemetry updates before the end-tick hook"));
    checks.are_equal(changed_observation.registry_entity_count,
                     changed_observation.telemetry_entity_count,
                     TEXT("Telemetry records the active registry count"));
    checks.are_equal(initial_active_entity_count - 1,
                     changed_observation.telemetry_entity_count,
                     TEXT("Killed entity changes the telemetry count"));

    auto const& final_observation{telemetry_observations.last_value()};
    checks.are_equal(telemetry_samples_before_change + 1,
                     final_observation.telemetry_sample_count,
                     TEXT("Unchanged ticks do not add telemetry samples"));
    checks.is_greater_than(final_observation.completed_ticks,
                           final_observation.last_telemetry_tick,
                           TEXT("Simulation continues after the last changed sample"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelTelemetryManagerTest,
                                 "Sandbox.UnitTests.LevelTelemetryManager.ActiveEntityCount",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

auto FLevelTelemetryManagerTest::RunTest(FString const&) -> bool {
    FTestEntityRegistry entity_registry;
    FLevelTelemetryManager telemetry_manager;

    telemetry_manager.initialise(entity_registry);
    auto const& initial_data{telemetry_manager.get_active_entity_count_data()};
    TestEqual(TEXT("Initialisation records one sample"), initial_data.num(), int32{1});
    TestEqual(TEXT("Initial sample uses tick zero"), initial_data.last_time(), uint64{0});
    TestEqual(TEXT("Initial sample records the active count"), initial_data.last_value(), 0);

    telemetry_manager.tick(1, entity_registry);
    TestEqual(TEXT("Unchanged count does not add a sample"), initial_data.num(), int32{1});

    telemetry_manager.reset();
    TestTrue(TEXT("Reset clears telemetry"), initial_data.is_empty());

    telemetry_manager.initialise(entity_registry);
    TestEqual(TEXT("Reinitialisation records one sample"), initial_data.num(), int32{1});
    TestEqual(TEXT("Reinitialisation returns to tick zero"), initial_data.last_time(), uint64{0});
    TestEqual(TEXT("Reinitialisation records the current count"), initial_data.last_value(), 0);

    return true;
}
