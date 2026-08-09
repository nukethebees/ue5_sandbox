#include "test_setup.h"
#include "TestActorSpawning.h"
#include "TestSimulationDriver.h"

#include <SandboxTests/cqtests/level_checks.h>
#include <SandboxTests/cqtests/SoftTestAssertions.h>

#include <SandboxCore/time_series_data.h>

#include <Sandbox/batch_game/SimulationConfig.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchGameUiData.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestMissionManager.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>
#include <Sandbox/batch_game/TestSpaceShip.h>
#include <Sandbox/ui/HUDManager.h>
#include <Sandbox/ui/ship_hud/ShipHudWidget.h>
#include <Sandbox/utilities/enums.h>

#include <CQTest.h>
#include <Engine/World.h>
#include <GameFramework/PlayerController.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/Optional.h>

TEST_CLASS(TestHUDManager, "Sandbox.FunctionalTests")
{
    using ThisClass = TestHUDManager;

    inline static FTimespan const timeout{0, 0, 2};
    static constexpr double early_sample_time{0.1};
    static constexpr double damage_queue_time{0.1};
    static constexpr double test_duration{0.35};

    struct FEntityCountSample {
        int32 cached_alive_count{0};
        int32 registered_hud_count{0};
    };
    struct FDefenceSample {
        ETestMissionState mission_state{ETestMissionState::NotStarted};
        int32 defended_entity_health{INDEX_NONE};
        float mission_stopwatch{0.f};
        int32 registered_hud_count{0};
    };
    struct FPlayerSample {
        int32 points{0};
        int32 registered_hud_count{0};
    };
    struct FMissionTimeSample {
        float cached_time{0.f};
        float mission_time{0.f};
        int32 registered_hud_count{0};
    };

    ml::FTestBatchOrchestratorLevelSetup level_setup;
    ml::FSoftTestAssertions checks{};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};
    int32 initial_alive_count{0};
    ml::TimeSeriesData<FEntityCountSample> entity_count_samples;
    ml::TimeSeriesData<FDefenceSample> defence_samples;
    ml::TimeSeriesData<FPlayerSample> player_samples;
    ml::TimeSeriesData<FMissionTimeSample> mission_time_samples;

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        test_driver.Reset();
        initial_alive_count = 0;
        entity_count_samples = {};
        defence_samples = {};
        player_samples = {};
        mission_time_samples = {};
    }

    AFTER_EACH()
    {
        if (auto* const orchestrator{level_setup.get_orchestrator()}; IsValid(orchestrator)) {
            orchestrator->clear_end_tick_test_hook();
        }
        level_setup.teardown();
    }

    TEST_METHOD(InitialCachesPopulateWithoutHUD)
    {
        level_setup.setup(TestCommandBuilder, *TestRunner);
        TestCommandBuilder.Do([this] { check_initial_caches(); });
    }

    TEST_METHOD(EntityCountPollingContinuesWithoutHUD)
    {
        level_setup.setup(TestCommandBuilder,
                          *TestRunner,
                          [](UWorld& world, UTestSimulationConfig const& config) {
                              ml::spawn_capital_proxy(world, config, FVector::ZeroVector);
                          });
        TestCommandBuilder.Do([this] { begin_entity_count_polling_scenario(); })
            .Until([this] { return test_driver->timeline.is_finished(); }, timeout)
            .Then([this] { check_entity_count_polling_result(); });
    }

    TEST_METHOD(MissionAndDefenceDataUpdateWithoutHUD)
    {
        level_setup.setup(TestCommandBuilder, *TestRunner, &ThisClass::configure_defence_mission);
        TestCommandBuilder.Do([this] { begin_defence_scenario(); })
            .Until([this] { return test_driver->timeline.is_finished(); }, timeout)
            .Then([this] { check_defence_result(); });
    }

    TEST_METHOD(PlayerStateAndKillsUpdateWithoutHUD)
    {
        level_setup.setup(TestCommandBuilder,
                          *TestRunner,
                          [](UWorld& world, UTestSimulationConfig const& config) {
                              ml::spawn_capital_proxy(world, config, FVector{2000.f, 0.f, 0.f});
                          });
        TestCommandBuilder.Do([this] { begin_player_kill_scenario(); })
            .Until([this] { return test_driver->timeline.is_finished(); }, timeout)
            .Then([this] { check_player_kill_result(); });
    }

    TEST_METHOD(MissionTimeUsesSimulationClockWithoutHUD)
    {
        level_setup.setup(TestCommandBuilder, *TestRunner, &ThisClass::configure_timed_mission);
        TestCommandBuilder.Do([this] { begin_mission_time_scenario(); })
            .Until([this] { return test_driver->timeline.is_finished(); }, timeout)
            .Then([this] { check_mission_time(); });
    }

    TEST_METHOD(LateHUDRegistrationSynchronisesAndUnregisters)
    {
        level_setup.setup(TestCommandBuilder, *TestRunner);
        TestCommandBuilder.Do([this] { check_hud_registration_lifecycle(); });
    }
  private:
    /* ------------------------------------------------------------------------------------------ */
    // Initial cache
    /* ------------------------------------------------------------------------------------------ */
    void check_initial_caches() {
        auto* const orchestrator{level_setup.get_orchestrator()};
        if (!checks.not_nullptr(orchestrator, TEXT("Orchestrator is available"))) {
            return;
        }

        auto const& hud_manager{orchestrator->get_hud_manager()};
        auto const* const registry{orchestrator->get_entity_registry()};
        auto const* const mission_manager{orchestrator->get_mission_manager()};
        auto const* const player_ship{orchestrator->get_player_ship()};
        check(registry);
        check(mission_manager);

        checks.are_equal(
            0, hud_manager.get_registered_hud_count(), TEXT("No HUD widgets are registered"));
        checks.are_equal(
            EHUDManagerState::Active, hud_manager.get_state(), TEXT("HUD manager is active"));
        checks.are_equal(registry->get_num_alive_active_entities(),
                         count_cached_entities(hud_manager),
                         TEXT("Initial entity count cache matches the registry"));

        auto const& mission_data{hud_manager.get_mission_data()};
        checks.are_equal(mission_manager->get_mission_state(),
                         mission_data.status_data.mission_state,
                         TEXT("Mission state is cached"));
        checks.are_equal(mission_manager->get_mission_stopwatch(),
                         mission_data.status_data.mission_stopwatch,
                         TEXT("Mission time is cached"));

        if (IsValid(player_ship)) {
            auto const& player_data{hud_manager.get_player_status_data()};
            checks.is_true(player_data.has_player_ship, TEXT("Player HUD state is available"));
            auto const player_health{player_ship->get_health_info()};
            checks.are_equal(player_health.health,
                             player_data.health.health,
                             TEXT("Player current health is cached"));
            checks.are_equal(player_health.max_health,
                             player_data.health.max_health,
                             TEXT("Player maximum health is cached"));
            checks.are_equal(
                player_ship->get_speed(), player_data.speed, TEXT("Player speed is cached"));
            checks.are_equal(player_ship->get_target_speed(),
                             player_data.target_speed,
                             TEXT("Player target speed is cached"));
        }

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }

    /* ------------------------------------------------------------------------------------------ */
    // Defence mission
    /* ------------------------------------------------------------------------------------------ */
    static void configure_defence_mission(UWorld & world, UTestSimulationConfig const& config) {
        auto& defended{ml::spawn_capital_proxy(world, config, FVector::ZeroVector)};
        auto* const mission_manager{world.SpawnActorDeferred<ATestMissionManager>(
            config.actor_classes.mission_manager_class, FTransform::Identity)};
        check(mission_manager);
        mission_manager->set_save_mission_results(false);
        mission_manager->set_mission_mode(ETestMissionMode::SurviveTime);
        mission_manager->set_target_time(10.f);
        mission_manager->add_entity_that_must_survive(defended);
        UGameplayStatics::FinishSpawningActor(mission_manager, FTransform::Identity);
    }
    void begin_defence_scenario() {
        test_driver = ml::TestSimulationDriver::from_world(level_setup.get_world());
        auto const* const mission_manager{test_driver->orchestrator.get_mission_manager()};
        check(mission_manager);

        auto const& initial_data{test_driver->orchestrator.get_hud_manager().get_mission_data()};
        checks.are_equal(1,
                         initial_data.static_data.surviving_entity_ids.Num(),
                         TEXT("Must-survive ID is cached"));
        checks.are_equal(1,
                         initial_data.static_data.surviving_entity_types.Num(),
                         TEXT("Must-survive type is cached"));
        checks.are_equal(1,
                         initial_data.status_data.surviving_entity_health.Num(),
                         TEXT("Must-survive health is cached"));
        checks.is_true(initial_data.status_data.surviving_entity_health[0].health > 0,
                       TEXT("Must-survive entity starts healthy"));

        auto const handles{mission_manager->get_entity_handles_that_must_survive()};
        check(handles.Num() == 1);
        test_driver->timeline.then_after(damage_queue_time,
                                         [this, handles] { test_driver->queue_kills(handles); });
        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_defence_tick));
        test_driver->timeline.finish_at(test_duration);
        test_driver->orchestrator.start_simulation();
    }
    void on_defence_tick(ATestBatchOrchestrator&) {
        auto const& data{test_driver->orchestrator.get_hud_manager().get_mission_data()};
        FDefenceSample sample{};
        sample.mission_state = data.status_data.mission_state;
        sample.mission_stopwatch = data.status_data.mission_stopwatch;
        sample.registered_hud_count =
            test_driver->orchestrator.get_hud_manager().get_registered_hud_count();
        if (data.status_data.surviving_entity_health.Num() == 1) {
            sample.defended_entity_health = data.status_data.surviving_entity_health[0].health;
        }
        defence_samples.add(test_driver->get_time(), sample);
        test_driver->timeline.tick(test_driver->get_time());
    }
    void check_defence_result() {
        ml::check_samples_recorded(defence_samples.num(), checks, TEXT("Defence samples recorded"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        auto const& sample{defence_samples.nearest_value(test_duration)};
        checks.are_equal(ETestMissionState::Failed,
                         sample.mission_state,
                         TEXT("Defence mission failure is cached"));
        checks.are_equal(
            0, sample.defended_entity_health, TEXT("Destroyed must-survive health is cached"));
        checks.is_true(sample.mission_stopwatch > 0.f,
                       TEXT("Mission stopwatch follows simulation time"));
        checks.are_equal(0, sample.registered_hud_count, TEXT("Mission updates without a HUD"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }

    /* ------------------------------------------------------------------------------------------ */
    // Timed mission
    /* ------------------------------------------------------------------------------------------ */
    static void configure_timed_mission(UWorld & world, UTestSimulationConfig const& config) {
        auto* const mission_manager{world.SpawnActorDeferred<ATestMissionManager>(
            config.actor_classes.mission_manager_class, FTransform::Identity)};
        check(mission_manager);
        mission_manager->set_save_mission_results(false);
        mission_manager->set_mission_mode(ETestMissionMode::SurviveTime);
        mission_manager->set_target_time(10.f);
        UGameplayStatics::FinishSpawningActor(mission_manager, FTransform::Identity);
    }
    void begin_mission_time_scenario() {
        test_driver = ml::TestSimulationDriver::from_world(level_setup.get_world());
        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_mission_time_tick));
        test_driver->timeline.finish_at(test_duration);
        test_driver->orchestrator.start_simulation();
    }
    void on_mission_time_tick(ATestBatchOrchestrator & orchestrator) {
        auto const* const mission_manager{orchestrator.get_mission_manager()};
        check(mission_manager);

        auto const& hud_manager{orchestrator.get_hud_manager()};
        auto const& mission_data{hud_manager.get_mission_data()};
        mission_time_samples.add(test_driver->get_time(),
                                 FMissionTimeSample{mission_data.status_data.mission_stopwatch,
                                                    mission_manager->get_mission_stopwatch(),
                                                    hud_manager.get_registered_hud_count()});
        test_driver->timeline.tick(test_driver->get_time());
    }
    void check_mission_time() {
        ml::check_samples_recorded(
            mission_time_samples.num(), checks, TEXT("Mission-time samples recorded"));
        if (mission_time_samples.is_empty()) {
            SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
            return;
        }

        auto const& sample{mission_time_samples.nearest_value(test_duration)};

        checks.is_true(sample.cached_time > 0.f, TEXT("Cached mission time advances"));
        checks.is_true(sample.mission_time - sample.cached_time <= 0.3f,
                       TEXT("Cached mission time follows the mission manager cadence"));
        checks.is_true(mission_time_samples.nearest_time(test_duration) - sample.cached_time <= 0.3,
                       TEXT("Cached mission time follows simulation-clock cadence"));
        checks.are_equal(
            0, sample.registered_hud_count, TEXT("Mission time updates without a HUD"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }

    /* ------------------------------------------------------------------------------------------ */
    // Player kill
    /* ------------------------------------------------------------------------------------------ */
    void begin_player_kill_scenario() {
        test_driver = ml::TestSimulationDriver::from_world(level_setup.get_world());
        auto const& player_ship{test_driver->get_player_ship()};
        auto const& capitals{test_driver->get_capital_ships()};
        check(capitals.get_num_instances() == 1);

        TArray<FRegistryEntityHandle> const targets{capitals.get_handle(0)};
        auto const instigator{player_ship.get_entity_handle()};
        test_driver->timeline.then_after(damage_queue_time, [this, targets, instigator] {
            test_driver->queue_kills(targets, instigator);
        });
        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_player_tick));
        test_driver->timeline.finish_at(test_duration);
        test_driver->orchestrator.start_simulation();
    }
    void on_player_tick(ATestBatchOrchestrator & orchestrator) {
        auto const& hud_manager{orchestrator.get_hud_manager()};
        auto const& data{hud_manager.get_player_status_data()};
        player_samples.add(test_driver->get_time(),
                           FPlayerSample{data.points, hud_manager.get_registered_hud_count()});
        test_driver->timeline.tick(test_driver->get_time());
    }
    void check_player_kill_result() {
        ml::check_samples_recorded(player_samples.num(), checks, TEXT("Player samples recorded"));
        if (player_samples.is_empty()) {
            SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
            return;
        }

        auto const& sample{player_samples.nearest_value(test_duration)};
        checks.are_equal(1, sample.points, TEXT("Player kill count is cached"));
        checks.are_equal(0, sample.registered_hud_count, TEXT("Player data updates without a HUD"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }

    /* ------------------------------------------------------------------------------------------ */
    // Registration
    /* ------------------------------------------------------------------------------------------ */
    void check_hud_registration_lifecycle() {
        auto* const orchestrator{level_setup.get_orchestrator()};
        if (!checks.not_nullptr(orchestrator, TEXT("Orchestrator is available"))) {
            return;
        }
        auto& hud_manager{orchestrator->get_hud_manager()};
        checks.are_equal(orchestrator->get_entity_registry()->get_num_alive_active_entities(),
                         count_cached_entities(hud_manager),
                         TEXT("Cache exists before HUD registration"));

        auto* const ui_data{ml::test_batch_game_ui_data::get_data_asset()};
        auto* const player_controller{level_setup.get_world().GetFirstPlayerController()};
        auto const ui_data_loaded{checks.not_nullptr(ui_data, TEXT("HUD UI data loads"))};
        auto const player_controller_available{
            checks.not_nullptr(player_controller, TEXT("Player controller is available"))};
        if (!ui_data_loaded || !player_controller_available) {
            return;
        }

        auto const widget_class{ui_data->get_widget_class<UShipHudWidget>()};
        if (!checks.is_true(static_cast<bool>(widget_class),
                            TEXT("Project HUD class is configured"))) {
            return;
        }
        auto* const hud{
            CreateWidget<UShipHudWidget>(player_controller, widget_class, TEXT("test_hud"))};
        if (!checks.not_nullptr(hud, TEXT("Real project HUD is created"))) {
            return;
        }

        hud->set_crosshair_distances(ui_data->crosshair_distances);
        hud->AddToViewport();
        hud_manager.register_hud(*hud);
        checks.are_equal(1,
                         hud_manager.get_registered_hud_count(),
                         TEXT("Late HUD is registered and synchronised immediately"));

        hud_manager.unregister_hud(*hud);
        checks.are_equal(
            0, hud_manager.get_registered_hud_count(), TEXT("HUD unregisters cleanly"));
        hud->RemoveFromParent();

        hud_manager.set_selected_mapping_context(FString{TEXT("after_unregister")});
        hud_manager.tick();
        checks.are_equal(0,
                         hud_manager.get_registered_hud_count(),
                         TEXT("Updates do not target the unregistered HUD"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }

    /* ------------------------------------------------------------------------------------------ */
    // Polling scenario
    /* ------------------------------------------------------------------------------------------ */
    void begin_entity_count_polling_scenario() {
        test_driver = ml::TestSimulationDriver::from_world(level_setup.get_world());
        auto const& hud_manager{test_driver->orchestrator.get_hud_manager()};
        initial_alive_count = count_cached_entities(hud_manager);
        checks.are_equal(0,
                         hud_manager.get_registered_hud_count(),
                         TEXT("Entity cache starts with no registered HUD"));

        auto const& capitals{test_driver->get_capital_ships()};
        check(capitals.get_num_instances() == 1);
        TArray<FRegistryEntityHandle> const targets{capitals.get_handle(0)};
        test_driver->timeline.then_after(damage_queue_time,
                                         [this, targets] { test_driver->queue_kills(targets); });
        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_entity_count_tick));
        test_driver->timeline.finish_at(test_duration);
        test_driver->orchestrator.start_simulation();
    }
    void check_entity_count_polling_result() {
        ml::check_samples_recorded(
            entity_count_samples.num(), checks, TEXT("Entity-count samples recorded"));
        if (entity_count_samples.is_empty()) {
            SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
            return;
        }

        auto const& early_sample{entity_count_samples.nearest_value(early_sample_time)};
        auto const& final_sample{entity_count_samples.nearest_value(test_duration)};
        checks.are_equal(initial_alive_count,
                         early_sample.cached_alive_count,
                         TEXT("Entity cache waits for its polling period"));
        checks.are_equal(initial_alive_count - 1,
                         final_sample.cached_alive_count,
                         TEXT("Entity cache updates after the polling period"));
        checks.are_equal(
            0, final_sample.registered_hud_count, TEXT("Entity cache still has no registered HUD"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }
    void on_entity_count_tick(ATestBatchOrchestrator & orchestrator) {
        entity_count_samples.add(
            test_driver->get_time(),
            FEntityCountSample{count_cached_entities(orchestrator.get_hud_manager()),
                               orchestrator.get_hud_manager().get_registered_hud_count()});
        test_driver->timeline.tick(test_driver->get_time());
    }

    /* ------------------------------------------------------------------------------------------ */
    // Shared
    /* ------------------------------------------------------------------------------------------ */
    static auto count_cached_entities(FHUDManager const& manager) -> int32 {
        int32 total{0};
        auto const& counts{manager.get_entity_count_data().alive_per_team_and_type};
        constexpr auto n_teams{ml::EnumCountTrait<ETestTeam>::count_value};
        constexpr auto n_types{ml::EnumCountTrait<ETestEntityType>::count_value};
        for (int32 team{0}; team < n_teams; ++team) {
            for (int32 type{0}; type < n_types; ++type) {
                total += counts[team][type];
            }
        }
        return total;
    }
};
