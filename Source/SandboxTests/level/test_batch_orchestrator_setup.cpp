#include <SandboxTests/support/test_setup.h>
#include "test_batch_orchestrator_setup_scenario.h"

#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/TestActorSpawning.h>

#include <SpaceGame/combat/lasers/TestLasersSimulation.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnersSimulation.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsSimulation.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/missions/TestMissionManager.h>
#include <SpaceGame/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersSimulation.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/LevelTelemetryManager.h>
#include <SpaceGame/simulation/SimulationClockInterface.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxCoreEngine/actor_utils.h>
#include <SandboxISMCComponent.h>

#include <Misc/AutomationTest.h>

namespace ml {
FTestBatchOrchestratorSetupScenario::FTestBatchOrchestratorSetupScenario(
    FSimulationTestContext& context, EOrchestratorSetupScenario const scenario)
    : FSimulationTestScenario{context}
    , scenario_{scenario} {}

void FTestBatchOrchestratorSetupScenario::run() {
    switch (scenario_) {
        case EOrchestratorSetupScenario::SpawnMissingActors:
            prepare_level();
            break;
        case EOrchestratorSetupScenario::SimulationClockConversions:
            simulation_clock_conversions();
            break;
        case EOrchestratorSetupScenario::LevelTelemetry:
            level_telemetry();
            break;
    }
}

void FTestBatchOrchestratorSetupScenario::prepare_level() {
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

        auto const resources{orchestrator->get_presentation_resources()};
        TestRunner->TestTrue(TEXT("Batch components are available"), resources.is_valid());
        TestRunner->TestTrue(TEXT("Orchestrator owns the laser batch component"),
                             resources.lasers->GetOwner() == orchestrator);
        for (auto* component :
             {resources.capital_ships, resources.fighters, resources.turrets, resources.spinners}) {
            TestRunner->TestTrue(TEXT("Orchestrator owns each batch component"),
                                 component->GetOwner() == orchestrator);
        }
        TestRunner->TestNull(TEXT("Simulation construction is deferred"),
                             orchestrator->get_level_simulation());
        initialise_test_driver();

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

        FSimulationClock clock;
        clock.initialise(FFixedTickLoop{});

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
    auto* const player{spawn_player_ship(
        context_.world, context_.config.classes.player_ship_class, &context_.config.player_ship)};
    if (!checks.is_valid(player, TEXT("Telemetry player ship is spawned"))) {
        return;
    }
    context_.orchestrator.set_player_ship(*player);
    telemetry_player_team_index = std::to_underlying(player->get_team());

    initialise_test_driver();
    telemetry_observations.reset();
    telemetry_observations.reserve(16);

    test_driver->orchestrator.start_simulation();
    initial_active_entity_count = test_driver->get_registry().get_num_alive_active_entities();
    initial_registry_slot_count = test_driver->get_registry().get_num_elements();
    initial_issued_unique_id_count = test_driver->get_registry().get_num_unique_ids_issued();
    test_driver->orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateRaw(
        this, &FTestBatchOrchestratorSetupScenario::on_level_telemetry_end_tick));
    test_driver->timeline.then_after(0.05, [this] { kill_telemetry_test_entity(); });
    test_driver->timeline.finish_at(0.15);
}

void FTestBatchOrchestratorSetupScenario::kill_telemetry_test_entity() {
    auto const& telemetry_manager{test_driver->orchestrator.get_level_telemetry_manager()};
    telemetry_samples_before_change = telemetry_manager.get_active_entity_count_data().num();
    composition_samples_before_change = telemetry_manager.get_active_entity_counts_data().num();
    kill_samples_before_change = telemetry_manager.get_cumulative_kill_count_data().num();

    TStaticArray<FRegistryEntityHandle, 1> const targets{
        test_driver->get_player_ship().get_entity_handle()};
    test_driver->queue_kills(targets, targets[0]);
}

void FTestBatchOrchestratorSetupScenario::on_level_telemetry_end_tick(
    ATestBatchOrchestrator& orchestrator) {
    auto const& telemetry_manager{orchestrator.get_level_telemetry_manager()};
    auto const& entity_count_data{telemetry_manager.get_active_entity_count_data()};
    auto const& entity_counts_data{telemetry_manager.get_active_entity_counts_data()};
    auto const& kill_count_data{telemetry_manager.get_cumulative_kill_count_data()};
    auto const& slot_count_data{telemetry_manager.get_registry_slot_count_data()};
    auto const& issued_unique_id_count_data{telemetry_manager.get_issued_unique_id_count_data()};
    check(!entity_count_data.is_empty());
    check(!entity_counts_data.is_empty());
    check(!kill_count_data.is_empty());
    check(!slot_count_data.is_empty());
    check(!issued_unique_id_count_data.is_empty());

    auto const player_type{std::to_underlying(ETestEntityType::PlayerShip)};
    auto const& telemetry_entity_counts{entity_counts_data.last_value()};
    auto const registry_entity_counts{test_driver->get_registry().count_alive_per_team_and_type()};

    telemetry_observations.add(
        test_driver->get_time(),
        FTelemetryObservation{
            .completed_ticks = orchestrator.get_completed_ticks(),
            .telemetry_sample_count = entity_count_data.num(),
            .last_telemetry_tick = entity_count_data.last_time(),
            .telemetry_entity_count = entity_count_data.last_value(),
            .registry_entity_count = test_driver->get_registry().get_num_alive_active_entities(),
            .composition_sample_count = entity_counts_data.num(),
            .last_composition_tick = entity_counts_data.last_time(),
            .telemetry_player_ship_count =
                telemetry_entity_counts[telemetry_player_team_index][player_type],
            .registry_player_ship_count =
                registry_entity_counts[telemetry_player_team_index][player_type],
            .kill_sample_count = kill_count_data.num(),
            .last_kill_tick = kill_count_data.last_time(),
            .cumulative_kill_count = kill_count_data.last_value(),
            .slot_sample_count = slot_count_data.num(),
            .registry_slot_count = slot_count_data.last_value(),
            .issued_unique_id_sample_count = issued_unique_id_count_data.num(),
            .issued_unique_id_count = issued_unique_id_count_data.last_value(),
        });
    test_driver->advance_timeline();
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
    checks.are_equal(int32{1},
                     initial_observation.composition_sample_count,
                     TEXT("Tick-zero composition baseline recorded"));
    checks.are_equal(uint64{0},
                     initial_observation.last_composition_tick,
                     TEXT("Composition baseline uses tick zero"));
    checks.are_equal(initial_observation.registry_player_ship_count,
                     initial_observation.telemetry_player_ship_count,
                     TEXT("Baseline composition records the player ship"));
    checks.are_equal(
        int32{1}, initial_observation.kill_sample_count, TEXT("Tick-zero kill baseline recorded"));
    checks.are_equal(
        uint64{0}, initial_observation.last_kill_tick, TEXT("Kill baseline uses tick zero"));
    checks.are_equal(
        int32{0}, initial_observation.cumulative_kill_count, TEXT("Kill baseline starts at zero"));
    checks.are_equal(
        int32{1}, initial_observation.slot_sample_count, TEXT("Tick-zero slot baseline recorded"));
    checks.are_equal(initial_registry_slot_count,
                     initial_observation.registry_slot_count,
                     TEXT("Slot baseline matches the registry"));
    checks.are_equal(int32{1},
                     initial_observation.issued_unique_id_sample_count,
                     TEXT("Tick-zero issued-ID baseline recorded"));
    checks.are_equal(initial_issued_unique_id_count,
                     initial_observation.issued_unique_id_count,
                     TEXT("Issued-ID baseline matches the registry"));

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
    checks.are_equal(composition_samples_before_change + 1,
                     changed_observation.composition_sample_count,
                     TEXT("Killed entity changes composition telemetry"));
    checks.are_equal(changed_observation.completed_ticks,
                     changed_observation.last_composition_tick,
                     TEXT("Composition updates before the end-tick hook"));
    checks.are_equal(changed_observation.registry_player_ship_count,
                     changed_observation.telemetry_player_ship_count,
                     TEXT("Composition records the destroyed player ship"));
    checks.are_equal(int32{0},
                     changed_observation.telemetry_player_ship_count,
                     TEXT("Destroyed player is removed from composition"));
    checks.are_equal(kill_samples_before_change + 1,
                     changed_observation.kill_sample_count,
                     TEXT("Killed entity changes cumulative kill telemetry"));
    checks.are_equal(changed_observation.completed_ticks,
                     changed_observation.last_kill_tick,
                     TEXT("Kill telemetry updates before the end-tick hook"));
    checks.are_equal(int32{1},
                     changed_observation.cumulative_kill_count,
                     TEXT("Killed entity increments the cumulative kill count"));
    checks.are_equal(int32{1},
                     changed_observation.slot_sample_count,
                     TEXT("Killing an entity does not change slot telemetry"));
    checks.are_equal(initial_registry_slot_count,
                     changed_observation.registry_slot_count,
                     TEXT("Killing an entity preserves registry slots"));
    checks.are_equal(int32{1},
                     changed_observation.issued_unique_id_sample_count,
                     TEXT("Killing an entity does not change issued-ID telemetry"));
    checks.are_equal(initial_issued_unique_id_count,
                     changed_observation.issued_unique_id_count,
                     TEXT("Killing an entity preserves issued IDs"));

    auto const& final_observation{telemetry_observations.last_value()};
    checks.are_equal(telemetry_samples_before_change + 1,
                     final_observation.telemetry_sample_count,
                     TEXT("Unchanged ticks do not add telemetry samples"));
    checks.is_greater_than(final_observation.completed_ticks,
                           final_observation.last_telemetry_tick,
                           TEXT("Simulation continues after the last changed sample"));

    auto const snapshot{test_driver->orchestrator.get_level_telemetry_manager().make_snapshot(
        final_observation.completed_ticks, test_driver->orchestrator.get_tick_period())};
    checks.are_equal(final_observation.telemetry_entity_count,
                     snapshot.active_entities,
                     TEXT("Snapshot records the final active entity count"));
    checks.are_equal(initial_issued_unique_id_count,
                     snapshot.spawned_entities,
                     TEXT("Snapshot records issued entities as spawned"));
    checks.are_equal(snapshot.spawned_entities - snapshot.active_entities,
                     snapshot.destroyed_entities,
                     TEXT("Snapshot derives destroyed entities from spawned and active counts"));
    checks.are_equal(int32{1}, snapshot.kills, TEXT("Snapshot records the observed kill"));
    checks.are_equal(final_observation.completed_ticks,
                     snapshot.active_entity_count_data.last_time(),
                     TEXT("Snapshot extends active entities to the completed tick"));
    checks.are_equal(final_observation.completed_ticks,
                     snapshot.cumulative_kill_count_data.last_time(),
                     TEXT("Snapshot extends kills to the completed tick"));
    for (auto const value : snapshot.active_entity_count_data.values()) {
        checks.is_true(value >= 0, TEXT("Snapshot active-entity samples are nonnegative"));
    }
    for (auto const value : snapshot.cumulative_kill_count_data.values()) {
        checks.is_true(value >= 0, TEXT("Snapshot kill samples are nonnegative"));
    }
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelTelemetryManagerTest,
                                 "Sandbox.UnitTests.LevelTelemetryManager",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

auto FLevelTelemetryManagerTest::RunTest(FString const&) -> bool {
    FTestEntityRegistry entity_registry;
    FLevelTelemetryManager telemetry_manager;

    telemetry_manager.initialise(entity_registry, {});
    auto const& active_count_data{telemetry_manager.get_active_entity_count_data()};
    auto const& active_counts_data{telemetry_manager.get_active_entity_counts_data()};
    auto const& kill_count_data{telemetry_manager.get_cumulative_kill_count_data()};
    auto const& slot_count_data{telemetry_manager.get_registry_slot_count_data()};
    auto const& issued_unique_id_count_data{telemetry_manager.get_issued_unique_id_count_data()};
    auto const& active_laser_count_data{telemetry_manager.get_active_laser_count_data()};
    auto const& cumulative_laser_spawn_count_data{
        telemetry_manager.get_cumulative_laser_spawn_count_data()};
    TestEqual(
        TEXT("Initialisation records one active-count sample"), active_count_data.num(), int32{1});
    TestEqual(TEXT("Initial active-count sample uses tick zero"),
              active_count_data.last_time(),
              uint64{0});
    TestEqual(TEXT("Initial active-count sample records zero"), active_count_data.last_value(), 0);
    TestEqual(
        TEXT("Initialisation records one composition sample"), active_counts_data.num(), int32{1});
    TestEqual(TEXT("Initial composition sample uses tick zero"),
              active_counts_data.last_time(),
              uint64{0});
    TestEqual(TEXT("Initial composition has no player ships"),
              active_counts_data.last_value()[std::to_underlying(ETestTeam::Green)]
                                             [std::to_underlying(ETestEntityType::PlayerShip)],
              0);
    TestEqual(TEXT("Initialisation records one kill sample"), kill_count_data.num(), int32{1});
    TestEqual(TEXT("Initial kill sample records zero"), kill_count_data.last_value(), 0);
    TestEqual(TEXT("Initialisation records one slot sample"), slot_count_data.num(), int32{1});
    TestEqual(TEXT("Initial slot sample records zero"), slot_count_data.last_value(), 0);
    TestEqual(TEXT("Initialisation records one issued-ID sample"),
              issued_unique_id_count_data.num(),
              int32{1});
    TestEqual(
        TEXT("Initial issued-ID sample records zero"), issued_unique_id_count_data.last_value(), 0);
    TestEqual(TEXT("Initialisation records one active-laser sample"),
              active_laser_count_data.num(),
              int32{1});
    TestEqual(TEXT("Initial active-laser sample records zero"),
              active_laser_count_data.last_value(),
              int32{0});
    TestEqual(TEXT("Initialisation records one fired-laser sample"),
              cumulative_laser_spawn_count_data.num(),
              int32{1});
    TestEqual(TEXT("Initial fired-laser sample records zero"),
              cumulative_laser_spawn_count_data.last_value(),
              int32{0});

    telemetry_manager.tick(1, entity_registry, {});
    TestEqual(
        TEXT("Unchanged active count does not add a sample"), active_count_data.num(), int32{1});
    TestEqual(
        TEXT("Unchanged composition does not add a sample"), active_counts_data.num(), int32{1});
    TestEqual(TEXT("Unchanged kills do not add a sample"), kill_count_data.num(), int32{1});
    TestEqual(TEXT("Unchanged slots do not add a sample"), slot_count_data.num(), int32{1});
    TestEqual(TEXT("Unchanged issued IDs do not add a sample"),
              issued_unique_id_count_data.num(),
              int32{1});
    TestEqual(TEXT("Unchanged active lasers do not add a sample"),
              active_laser_count_data.num(),
              int32{1});
    TestEqual(TEXT("Unchanged fired lasers do not add a sample"),
              cumulative_laser_spawn_count_data.num(),
              int32{1});

    telemetry_manager.tick(2, entity_registry, {.active_count = 3, .cumulative_spawn_count = 11});
    TestEqual(
        TEXT("Active laser changes are sampled"), active_laser_count_data.last_value(), int32{3});
    TestEqual(TEXT("Fired laser changes are sampled"),
              cumulative_laser_spawn_count_data.last_value(),
              int32{11});

    auto const laser_snapshot{telemetry_manager.make_snapshot(5, 0.25)};
    TestEqual(
        TEXT("Snapshot records elapsed simulation time"), laser_snapshot.elapsed_seconds, 1.25);
    TestEqual(TEXT("Snapshot records active lasers"), laser_snapshot.active_lasers, int32{3});
    TestEqual(TEXT("Snapshot records fired lasers"), laser_snapshot.lasers_fired, int32{11});
    TestEqual(TEXT("Snapshot extends active entities to its terminal tick"),
              laser_snapshot.active_entity_count_data.last_time(),
              uint64{5});
    TestEqual(TEXT("Snapshot extends kills to its terminal tick"),
              laser_snapshot.cumulative_kill_count_data.last_time(),
              uint64{5});

    telemetry_manager.reset();
    TestTrue(TEXT("Reset clears active-count telemetry"), active_count_data.is_empty());
    TestTrue(TEXT("Reset clears composition telemetry"), active_counts_data.is_empty());
    TestTrue(TEXT("Reset clears kill telemetry"), kill_count_data.is_empty());
    TestTrue(TEXT("Reset clears slot telemetry"), slot_count_data.is_empty());
    TestTrue(TEXT("Reset clears issued-ID telemetry"), issued_unique_id_count_data.is_empty());
    TestTrue(TEXT("Reset clears active-laser telemetry"), active_laser_count_data.is_empty());
    TestTrue(TEXT("Reset clears fired-laser telemetry"),
             cumulative_laser_spawn_count_data.is_empty());

    telemetry_manager.initialise(entity_registry, {});
    TestEqual(TEXT("Reinitialisation records one active-count sample"),
              active_count_data.num(),
              int32{1});
    TestEqual(TEXT("Reinitialisation returns active-count telemetry to tick zero"),
              active_count_data.last_time(),
              uint64{0});

    TArray<float> const locations_x{0.f, 0.f, 0.f};
    TArray<float> const locations_y{0.f, 0.f, 0.f};
    TArray<float> const locations_z{0.f, 0.f, 0.f};
    TArray<float> const velocities_x{0.f, 0.f, 0.f};
    TArray<float> const velocities_y{0.f, 0.f, 0.f};
    TArray<float> const velocities_z{0.f, 0.f, 0.f};
    TArray<float> const radii{0.f, 0.f, 0.f};
    TArray<int32> const healths{100, 100, 100};
    TArray<ETestTeam> const teams{ETestTeam::Green, ETestTeam::Red, ETestTeam::Red};
    TArray<ETestEntityType> const entity_types{
        ETestEntityType::PlayerShip,
        ETestEntityType::CapitalShip,
        ETestEntityType::CapitalShipFighter,
    };
    TArray<uint8> const alive{1u, 1u, 1u};
    FTestEntityRegistry::EntityData::ConstView const fixture_entity_data{
        .locations = FVectors3fConstView{locations_x, locations_y, locations_z},
        .velocities = FVectors3fConstView{velocities_x, velocities_y, velocities_z},
        .radii = radii,
        .healths = healths,
        .teams = teams,
        .entity_types = entity_types,
        .alive = alive,
    };
    entity_registry.add_entities(fixture_entity_data);

    telemetry_manager.tick(2, entity_registry, {});
    TestEqual(TEXT("Entity additions update active-count telemetry"),
              active_count_data.last_value(),
              int32{3});
    TestEqual(TEXT("Entity additions update active-count telemetry at their tick"),
              active_count_data.last_time(),
              uint64{2});
    TestEqual(TEXT("Composition records the green player ship"),
              active_counts_data.last_value()[std::to_underlying(ETestTeam::Green)]
                                             [std::to_underlying(ETestEntityType::PlayerShip)],
              1);
    TestEqual(TEXT("Composition records the red capital ship"),
              active_counts_data.last_value()[std::to_underlying(ETestTeam::Red)]
                                             [std::to_underlying(ETestEntityType::CapitalShip)],
              1);
    TestEqual(TEXT("Composition records the red fighter"),
              active_counts_data.last_value()[std::to_underlying(
                  ETestTeam::Red)][std::to_underlying(ETestEntityType::CapitalShipFighter)],
              1);
    TestEqual(
        TEXT("Entity additions update slot telemetry"), slot_count_data.last_value(), int32{3});
    TestEqual(TEXT("Entity additions update issued-ID telemetry"),
              issued_unique_id_count_data.last_value(),
              int32{3});

    telemetry_manager.tick(3, entity_registry, {});
    TestEqual(TEXT("Unchanged fixture does not add active-count telemetry"),
              active_count_data.num(),
              int32{2});
    TestEqual(TEXT("Unchanged fixture does not add composition telemetry"),
              active_counts_data.num(),
              int32{2});
    TestEqual(
        TEXT("Unchanged fixture does not add slot telemetry"), slot_count_data.num(), int32{2});
    TestEqual(TEXT("Unchanged fixture does not add issued-ID telemetry"),
              issued_unique_id_count_data.num(),
              int32{2});

    auto const entity_snapshot{telemetry_manager.make_snapshot(3, 0.5)};
    TestEqual(TEXT("Entity snapshot records elapsed time"), entity_snapshot.elapsed_seconds, 1.5);
    TestEqual(TEXT("Entity snapshot records spawned entities"),
              entity_snapshot.spawned_entities,
              int32{3});
    TestEqual(
        TEXT("Entity snapshot records active entities"), entity_snapshot.active_entities, int32{3});
    TestEqual(TEXT("Entity snapshot records no destroyed entities"),
              entity_snapshot.destroyed_entities,
              int32{0});
    TestEqual(TEXT("Entity snapshot records no kills"), entity_snapshot.kills, int32{0});
    for (auto const value : entity_snapshot.active_entity_count_data.values()) {
        TestTrue(TEXT("Entity snapshot active counts are nonnegative"), value >= 0);
    }
    for (auto const value : entity_snapshot.cumulative_kill_count_data.values()) {
        TestTrue(TEXT("Entity snapshot kill counts are nonnegative"), value >= 0);
    }

    return true;
}
