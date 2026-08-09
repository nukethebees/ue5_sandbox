#include "test_setup.h"
#include "TestSimulationDriver.h"

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
    static constexpr double settled_sample_time{0.35};

    ml::FTestBatchOrchestratorLevelSetup level_setup;
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};
    int32 initial_alive_count{0};
    bool checked_before_entity_count_poll{false};

    BEFORE_EACH()
    {
        test_driver.Reset();
        initial_alive_count = 0;
        checked_before_entity_count_poll = false;
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
                              ThisClass::spawn_capital_proxy(world, config, FVector::ZeroVector);
                          });
        TestCommandBuilder.Do([this] { begin_entity_count_polling_scenario(); })
            .Until([this] { return test_driver->get_time() >= settled_sample_time; }, timeout)
            .Then([this] { check_entity_count_polling_result(); });
    }

    TEST_METHOD(MissionAndDefenceDataUpdateWithoutHUD)
    {
        level_setup.setup(TestCommandBuilder, *TestRunner, &ThisClass::configure_defence_mission);
        TestCommandBuilder.Do([this] { begin_defence_scenario(); })
            .Until([this] { return defence_cache_has_updated(); }, timeout)
            .Then([this] { check_defence_result(); });
    }

    TEST_METHOD(PlayerStateAndKillsUpdateWithoutHUD)
    {
        level_setup.setup(TestCommandBuilder,
                          *TestRunner,
                          [](UWorld& world, UTestSimulationConfig const& config) {
                              ThisClass::spawn_capital_proxy(
                                  world, config, FVector{2000.f, 0.f, 0.f});
                          });
        TestCommandBuilder.Do([this] { begin_player_kill_scenario(); })
            .Until([this] { return player_points_cache_has_updated(); }, timeout)
            .Then([this] { check_player_kill_result(); });
    }

    TEST_METHOD(MissionTimeUsesSimulationClockWithoutHUD)
    {
        level_setup.setup(TestCommandBuilder, *TestRunner, &ThisClass::configure_timed_mission);
        TestCommandBuilder
            .Do([this] {
                test_driver = ml::TestSimulationDriver::from_world(level_setup.get_world());
                test_driver->orchestrator.start_simulation();
            })
            .Until([this] { return test_driver->get_time() >= settled_sample_time; }, timeout)
            .Then([this] { check_mission_time(); });
    }

    TEST_METHOD(LateHUDRegistrationSynchronisesAndUnregisters)
    {
        level_setup.setup(TestCommandBuilder, *TestRunner);
        TestCommandBuilder.Do([this] { check_hud_registration_lifecycle(); });
    }
  private:
    static auto spawn_capital_proxy(UWorld & world,
                                    UTestSimulationConfig const& config,
                                    FVector const& location) -> ATestCapitalShipProxy& {
        auto* const proxy{world.SpawnActorDeferred<ATestCapitalShipProxy>(
            ATestCapitalShipProxy::StaticClass(), FTransform{FRotator::ZeroRotator, location})};
        check(proxy);
        proxy->set_actor_config(config.simulation_config->capital_ships_config.Get());
        UGameplayStatics::FinishSpawningActor(proxy, FTransform{FRotator::ZeroRotator, location});
        return *proxy;
    }

    static void configure_defence_mission(UWorld & world, UTestSimulationConfig const& config) {
        auto& defended{spawn_capital_proxy(world, config, FVector::ZeroVector)};
        auto* const mission_manager{world.SpawnActorDeferred<ATestMissionManager>(
            config.actor_classes.mission_manager_class, FTransform::Identity)};
        check(mission_manager);
        mission_manager->set_save_mission_results(false);
        mission_manager->set_mission_mode(ETestMissionMode::SurviveTime);
        mission_manager->set_target_time(10.f);
        mission_manager->add_entity_that_must_survive(defended);
        UGameplayStatics::FinishSpawningActor(mission_manager, FTransform::Identity);
    }

    static void configure_timed_mission(UWorld & world, UTestSimulationConfig const& config) {
        auto* const mission_manager{world.SpawnActorDeferred<ATestMissionManager>(
            config.actor_classes.mission_manager_class, FTransform::Identity)};
        check(mission_manager);
        mission_manager->set_save_mission_results(false);
        mission_manager->set_mission_mode(ETestMissionMode::SurviveTime);
        mission_manager->set_target_time(10.f);
        UGameplayStatics::FinishSpawningActor(mission_manager, FTransform::Identity);
    }

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

    void check_initial_caches() {
        auto* const orchestrator{level_setup.get_orchestrator()};
        TestRunner->TestNotNull(TEXT("Orchestrator is available"), orchestrator);
        if (!orchestrator) {
            return;
        }

        auto const& hud_manager{orchestrator->get_hud_manager()};
        auto const* const registry{orchestrator->get_entity_registry()};
        auto const* const mission_manager{orchestrator->get_mission_manager()};
        auto const* const player_ship{orchestrator->get_player_ship()};
        check(registry);
        check(mission_manager);

        TestRunner->TestEqual(
            TEXT("No HUD widgets are registered"), hud_manager.get_registered_hud_count(), 0);
        TestRunner->TestEqual(
            TEXT("HUD manager is active"), hud_manager.get_state(), EHUDManagerState::Active);
        TestRunner->TestEqual(TEXT("Initial entity count cache matches the registry"),
                              count_cached_entities(hud_manager),
                              registry->get_num_alive_active_entities());

        auto const& mission_data{hud_manager.get_mission_data()};
        TestRunner->TestEqual(TEXT("Mission state is cached"),
                              mission_data.status_data.mission_state,
                              mission_manager->get_mission_state());
        TestRunner->TestEqual(TEXT("Mission time is cached"),
                              mission_data.status_data.mission_stopwatch,
                              mission_manager->get_mission_stopwatch());

        if (IsValid(player_ship)) {
            auto const& player_data{hud_manager.get_player_status_data()};
            TestRunner->TestTrue(TEXT("Player HUD state is available"),
                                 player_data.has_player_ship);
            auto const player_health{player_ship->get_health_info()};
            TestRunner->TestEqual(TEXT("Player current health is cached"),
                                  player_data.health.health,
                                  player_health.health);
            TestRunner->TestEqual(TEXT("Player maximum health is cached"),
                                  player_data.health.max_health,
                                  player_health.max_health);
            TestRunner->TestEqual(
                TEXT("Player speed is cached"), player_data.speed, player_ship->get_speed());
            TestRunner->TestEqual(TEXT("Player target speed is cached"),
                                  player_data.target_speed,
                                  player_ship->get_target_speed());
        }
    }

    void begin_entity_count_polling_scenario() {
        test_driver = ml::TestSimulationDriver::from_world(level_setup.get_world());
        auto const& hud_manager{test_driver->orchestrator.get_hud_manager()};
        initial_alive_count = count_cached_entities(hud_manager);
        TestRunner->TestEqual(TEXT("Entity cache starts with no registered HUD"),
                              hud_manager.get_registered_hud_count(),
                              0);

        auto const& capitals{test_driver->get_capital_ships()};
        check(capitals.get_num_instances() == 1);
        TArray<FRegistryEntityHandle> const targets{capitals.get_handle(0)};
        test_driver->queue_kills(targets);
        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_entity_count_tick));
        test_driver->orchestrator.start_simulation();
    }

    void on_entity_count_tick(ATestBatchOrchestrator & orchestrator) {
        auto const time{orchestrator.get_simulation_time()};
        if (!checked_before_entity_count_poll && time >= early_sample_time) {
            TestRunner->TestEqual(TEXT("Entity cache waits for its polling period"),
                                  count_cached_entities(orchestrator.get_hud_manager()),
                                  initial_alive_count);
            checked_before_entity_count_poll = true;
        }
    }

    void check_entity_count_polling_result() {
        auto& orchestrator{test_driver->orchestrator};
        TestRunner->TestTrue(TEXT("Pre-poll cache state was checked"),
                             checked_before_entity_count_poll);
        TestRunner->TestEqual(TEXT("Entity cache updates after the polling period"),
                              count_cached_entities(orchestrator.get_hud_manager()),
                              initial_alive_count - 1);
        TestRunner->TestEqual(TEXT("Entity cache still has no registered HUD"),
                              orchestrator.get_hud_manager().get_registered_hud_count(),
                              0);
    }

    void begin_defence_scenario() {
        test_driver = ml::TestSimulationDriver::from_world(level_setup.get_world());
        auto const* const mission_manager{test_driver->orchestrator.get_mission_manager()};
        check(mission_manager);

        auto const& initial_data{test_driver->orchestrator.get_hud_manager().get_mission_data()};
        TestRunner->TestEqual(TEXT("Must-survive ID is cached"),
                              initial_data.static_data.surviving_entity_ids.Num(),
                              1);
        TestRunner->TestEqual(TEXT("Must-survive type is cached"),
                              initial_data.static_data.surviving_entity_types.Num(),
                              1);
        TestRunner->TestEqual(TEXT("Must-survive health is cached"),
                              initial_data.status_data.surviving_entity_health.Num(),
                              1);
        TestRunner->TestTrue(TEXT("Must-survive entity starts healthy"),
                             initial_data.status_data.surviving_entity_health[0].health > 0);

        auto const handles{mission_manager->get_entity_handles_that_must_survive()};
        check(handles.Num() == 1);
        test_driver->queue_kills(handles);
        test_driver->orchestrator.start_simulation();
    }

    auto defence_cache_has_updated() const -> bool {
        auto const& data{test_driver->orchestrator.get_hud_manager().get_mission_data()};
        return data.status_data.mission_state == ETestMissionState::Failed &&
               data.status_data.surviving_entity_health.Num() == 1 &&
               data.status_data.surviving_entity_health[0].health == 0;
    }

    void check_defence_result() {
        auto const& hud_manager{test_driver->orchestrator.get_hud_manager()};
        auto const& data{hud_manager.get_mission_data()};
        TestRunner->TestEqual(TEXT("Defence mission failure is cached"),
                              data.status_data.mission_state,
                              ETestMissionState::Failed);
        TestRunner->TestEqual(TEXT("Destroyed must-survive health is cached"),
                              data.status_data.surviving_entity_health[0].health,
                              0);
        TestRunner->TestTrue(TEXT("Mission stopwatch follows simulation time"),
                             data.status_data.mission_stopwatch > 0.f);
        TestRunner->TestEqual(
            TEXT("Mission updates without a HUD"), hud_manager.get_registered_hud_count(), 0);
    }

    void begin_player_kill_scenario() {
        test_driver = ml::TestSimulationDriver::from_world(level_setup.get_world());
        auto const& player_ship{test_driver->get_player_ship()};
        auto const& capitals{test_driver->get_capital_ships()};
        check(capitals.get_num_instances() == 1);

        TArray<FRegistryEntityHandle> const targets{capitals.get_handle(0)};
        test_driver->queue_kills(targets, player_ship.get_entity_handle());
        test_driver->orchestrator.start_simulation();
    }

    auto player_points_cache_has_updated() const -> bool {
        return test_driver->orchestrator.get_hud_manager().get_player_status_data().points == 1;
    }

    void check_player_kill_result() {
        auto const& hud_manager{test_driver->orchestrator.get_hud_manager()};
        auto const& data{hud_manager.get_player_status_data()};
        TestRunner->TestEqual(TEXT("Player kill count is cached"), data.points, 1);
        TestRunner->TestEqual(
            TEXT("Player data updates without a HUD"), hud_manager.get_registered_hud_count(), 0);
    }

    void check_mission_time() {
        auto& orchestrator{test_driver->orchestrator};
        auto const* const mission_manager{orchestrator.get_mission_manager()};
        check(mission_manager);
        auto const cached_time{
            orchestrator.get_hud_manager().get_mission_data().status_data.mission_stopwatch};

        TestRunner->TestTrue(TEXT("Cached mission time advances"), cached_time > 0.f);
        TestRunner->TestTrue(TEXT("Cached mission time follows the mission manager cadence"),
                             mission_manager->get_mission_stopwatch() - cached_time <= 0.3f);
        TestRunner->TestTrue(TEXT("Cached mission time follows simulation-clock cadence"),
                             orchestrator.get_simulation_time() - cached_time <= 0.3);
        TestRunner->TestEqual(TEXT("Mission time updates without a HUD"),
                              orchestrator.get_hud_manager().get_registered_hud_count(),
                              0);
    }

    void check_hud_registration_lifecycle() {
        auto* const orchestrator{level_setup.get_orchestrator()};
        check(orchestrator);
        auto& hud_manager{orchestrator->get_hud_manager()};
        TestRunner->TestEqual(TEXT("Cache exists before HUD registration"),
                              count_cached_entities(hud_manager),
                              orchestrator->get_entity_registry()->get_num_alive_active_entities());

        auto* const ui_data{ml::test_batch_game_ui_data::get_data_asset()};
        auto* const player_controller{level_setup.get_world().GetFirstPlayerController()};
        TestRunner->TestNotNull(TEXT("HUD UI data loads"), ui_data);
        TestRunner->TestNotNull(TEXT("Player controller is available"), player_controller);
        if (!ui_data || !player_controller) {
            return;
        }

        auto const widget_class{ui_data->get_widget_class<UShipHudWidget>()};
        TestRunner->TestTrue(TEXT("Project HUD class is configured"),
                             static_cast<bool>(widget_class));
        if (!widget_class) {
            return;
        }
        auto* const hud{
            CreateWidget<UShipHudWidget>(player_controller, widget_class, TEXT("test_hud"))};
        TestRunner->TestNotNull(TEXT("Real project HUD is created"), hud);
        if (!hud) {
            return;
        }

        hud->set_crosshair_distances(ui_data->crosshair_distances);
        hud->AddToViewport();
        hud_manager.register_hud(*hud);
        TestRunner->TestEqual(TEXT("Late HUD is registered and synchronised immediately"),
                              hud_manager.get_registered_hud_count(),
                              1);

        hud_manager.unregister_hud(*hud);
        TestRunner->TestEqual(
            TEXT("HUD unregisters cleanly"), hud_manager.get_registered_hud_count(), 0);
        hud->RemoveFromParent();

        hud_manager.set_selected_mapping_context(FString{TEXT("after_unregister")});
        hud_manager.tick();
        TestRunner->TestEqual(TEXT("Updates do not target the unregistered HUD"),
                              hud_manager.get_registered_hud_count(),
                              0);
    }
};
