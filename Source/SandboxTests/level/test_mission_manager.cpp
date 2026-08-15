#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestSimulationDriver.h>

#include <SandboxTests/support/level_checks.h>
#include <SandboxTests/support/SoftTestAssertions.h>

#include <SandboxCore/time_series_data.h>

#include <Sandbox/batch_game/SimulationConfig.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestMissionManager.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>

#include <SandboxCoreEngine/actor_utils.h>

#include <Components/MapTestSpawner.h>
#include <CQTest.h>
#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/Optional.h>

TEST_CLASS(TestMissionManager, "Sandbox.LevelTests")
{
    using ThisClass = TestMissionManager;

    enum class EScenario : uint8 {
        SurviveTime,
        KillEnemies,
        KillEnemiesWithinTime,
        DefenceObjective,
    };

    struct FSimulationSample {
        ETestMissionState mission_state{ETestMissionState::NotStarted};
        ETestMissionFailReason mission_fail_reason{ETestMissionFailReason::None};
        int32 mission_kills{0};
        TArray<int32> surviving_entity_health;
    };

    static constexpr float short_mission_time{0.1f};
    static constexpr float long_mission_time{10.f};
    inline static FTimespan const timeout{0, 0, 2};

    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    TOptional<ml::FTestBatchOrchestratorLevelSetup> level_setup{NullOpt};
    ml::FSoftTestAssertions checks{};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};
    EScenario scenario{EScenario::SurviveTime};
    ml::TimeSeriesData<FSimulationSample> samples;

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        test_driver.Reset();
        samples = {};
        spawner = FMapTestSpawner::CreateFromTempLevel(TestCommandBuilder);
        level_setup.Emplace(*spawner, *TestRunner, checks);
    }

    AFTER_EACH()
    {
        if (test_driver.IsSet()) {
            test_driver->orchestrator.clear_end_tick_test_hook();
        }
        level_setup->teardown();
        level_setup.Reset();
        spawner.Reset();
    }
  private:
    static auto spawn_mission_manager(UWorld & world, UTestSimulationConfig const& config)
        -> ATestMissionManager& {
        auto* const manager{world.SpawnActorDeferred<ATestMissionManager>(
            config.actor_classes.mission_manager_class, FTransform::Identity)};
        check(manager);
        return *manager;
    }

    static void finish_spawning_mission_manager(ATestMissionManager & manager) {
        UGameplayStatics::FinishSpawningActor(&manager, FTransform::Identity);
    }

    static void configure_level(UWorld & world,
                                UTestSimulationConfig const& config,
                                ml::FSoftTestAssertions& checks,
                                EScenario const scenario) {
        auto* const first_capital{ml::spawn_capital_proxy(
            world, config, checks, FName{TEXT("hero_capital")}, FVector{-2000.f, 0.f, 0.f})};
        if (!IsValid(first_capital)) {
            return;
        }
        auto& manager{spawn_mission_manager(world, config)};

        manager.set_save_mission_results(false);

        switch (scenario) {
            case EScenario::SurviveTime: {
                manager.set_mission_mode(ETestMissionMode::SurviveTime);
                manager.set_target_time(short_mission_time);
                manager.add_entity_that_must_survive(*first_capital);
                break;
            }
            case EScenario::KillEnemies: {
                ml::spawn_capital_proxy(
                    world, config, checks, FName{TEXT("enemy_capital")}, FVector{2000.f, 0.f, 0.f});
                manager.set_mission_mode(ETestMissionMode::KillEnemies);
                manager.set_kill_target(1);
                manager.add_hero_entity(*first_capital);
                break;
            }
            case EScenario::KillEnemiesWithinTime: {
                ml::spawn_capital_proxy(
                    world, config, checks, FName{TEXT("enemy_capital")}, FVector{2000.f, 0.f, 0.f});
                manager.set_mission_mode(ETestMissionMode::KillEnemiesWithinTime);
                manager.set_target_time(short_mission_time);
                manager.set_kill_target(1);
                manager.add_hero_entity(*first_capital);
                break;
            }
            case EScenario::DefenceObjective: {
                manager.set_mission_mode(ETestMissionMode::SurviveTime);
                manager.set_target_time(long_mission_time);
                manager.add_entity_that_must_survive(*first_capital);
                break;
            }
            default: {
                checkNoEntry();
                break;
            }
        }

        finish_spawning_mission_manager(manager);
    }

    void setup_scenario(EScenario const new_scenario) {
        scenario = new_scenario;
        level_setup->setup(
            TestCommandBuilder,
            [this, new_scenario](UWorld& world, UTestSimulationConfig const& config) {
                configure_level(world, config, checks, new_scenario);
            });

        TestCommandBuilder.Do([this] { start_scenario(); })
            .Until([this] { return mission_has_ended(); }, timeout)
            .Then([this] { check_scenario_result(); });
    }

    void start_scenario() {
        auto& world{level_setup->get_world()};
        test_driver = ml::TestSimulationDriver::from_world(world);

        auto* const manager{ml::get_first_actor<ATestMissionManager>(world)};
        check(manager);

        TestRunner->TestEqual(TEXT("Mission starts running"),
                              manager->get_mission_state(),
                              ETestMissionState::Running);
        TestRunner->TestFalse(TEXT("Mission result saving is disabled"),
                              manager->should_save_mission_results());

        auto const surviving_health{manager->get_entity_health_that_must_survive()};
        TestRunner->TestEqual(TEXT("Survival health data matches objective handles"),
                              surviving_health.Num(),
                              manager->get_entity_handles_that_must_survive().Num());
        TestRunner->TestEqual(TEXT("Survival IDs match objective handles"),
                              manager->get_entity_ids_that_must_survive().Num(),
                              manager->get_entity_handles_that_must_survive().Num());
        TestRunner->TestEqual(TEXT("Survival types match objective handles"),
                              manager->get_entity_types_that_must_survive().Num(),
                              manager->get_entity_handles_that_must_survive().Num());
        if (!surviving_health.IsEmpty()) {
            TestRunner->TestTrue(TEXT("Survival health captures a positive maximum"),
                                 surviving_health[0].max_health > 0);
            TestRunner->TestEqual(TEXT("Initial survival health is full"),
                                  surviving_health[0].health,
                                  surviving_health[0].max_health);
        }

        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));
        if (scenario == EScenario::KillEnemies) {
            test_driver->timeline.then_after(0.01, [this, manager] { queue_enemy_kill(*manager); });
        } else if (scenario == EScenario::DefenceObjective) {
            test_driver->timeline.then_after(
                0.01, [this, manager] { queue_defended_entity_kill(*manager); });
        }
        test_driver->orchestrator.start_simulation();
    }

    void on_end_tick(ATestBatchOrchestrator&) {
        auto const* const manager{test_driver->orchestrator.get_mission_manager()};
        check(manager);

        FSimulationSample sample{};
        sample.mission_state = manager->get_mission_state();
        sample.mission_fail_reason = manager->get_mission_fail_reason();
        sample.mission_kills = manager->get_mission_kills();
        for (auto const& health : manager->get_entity_health_that_must_survive()) {
            sample.surviving_entity_health.Add(health.health);
        }

        samples.add(test_driver->get_time(), MoveTemp(sample));
        test_driver->timeline.tick(test_driver->get_time());
    }

    void queue_enemy_kill(ATestMissionManager const& manager) {
        auto const hero_handles{manager.get_hero_entity_handles()};
        check(hero_handles.Num() == 1);

        auto const hero{hero_handles[0]};
        auto const& capitals{test_driver->get_capital_ships()};
        auto const n{capitals.get_num_instances()};

        FRegistryEntityHandle enemy;
        for (int32 i{0}; i < n; ++i) {
            auto const handle{capitals.get_handle(i)};
            if (handle != hero) {
                enemy = handle;
                break;
            }
        }
        check(enemy.is_valid());

        TArray<FRegistryEntityHandle> const targets{enemy};
        test_driver->queue_kills(targets, hero);
    }

    void queue_defended_entity_kill(ATestMissionManager const& manager) {
        auto const defended_handles{manager.get_entity_handles_that_must_survive()};
        check(defended_handles.Num() == 1);

        TArray<FRegistryEntityHandle> const targets{defended_handles[0]};
        auto const damage{test_driver->get_capital_ships().get_health(defended_handles[0])};
        test_driver->queue_damage(targets, damage);
    }

    auto mission_has_ended() const -> bool {
        auto const* const manager{test_driver->orchestrator.get_mission_manager()};
        auto const state{manager->get_mission_state()};
        return state == ETestMissionState::Succeeded || state == ETestMissionState::Failed;
    }

    void check_scenario_result() {
        ml::check_samples_recorded(
            samples.num(), checks, TEXT("Mission simulation samples recorded"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        auto const& sample{samples.last_value()};

        switch (scenario) {
            case EScenario::SurviveTime: {
                TestRunner->TestEqual(TEXT("Survive-time mission succeeds"),
                                      sample.mission_state,
                                      ETestMissionState::Succeeded);
                TestRunner->TestEqual(TEXT("Successful mission has no failure reason"),
                                      sample.mission_fail_reason,
                                      ETestMissionFailReason::None);
                break;
            }
            case EScenario::KillEnemies: {
                TestRunner->TestEqual(TEXT("Kill mission succeeds"),
                                      sample.mission_state,
                                      ETestMissionState::Succeeded);
                TestRunner->TestEqual(
                    TEXT("Hero kill contributes to mission"), sample.mission_kills, 1);
                break;
            }
            case EScenario::KillEnemiesWithinTime: {
                TestRunner->TestEqual(TEXT("Timed kill mission fails"),
                                      sample.mission_state,
                                      ETestMissionState::Failed);
                TestRunner->TestEqual(TEXT("Timed mission reports elapsed time"),
                                      sample.mission_fail_reason,
                                      ETestMissionFailReason::TimeElapsed);
                break;
            }
            case EScenario::DefenceObjective: {
                TestRunner->TestEqual(TEXT("Defence objective failure fails mission"),
                                      sample.mission_state,
                                      ETestMissionState::Failed);
                TestRunner->TestEqual(TEXT("Defence failure reason is retained"),
                                      sample.mission_fail_reason,
                                      ETestMissionFailReason::DefenceObjectiveFailed);
                TestRunner->TestTrue(TEXT("Defence health is sampled"),
                                     !sample.surviving_entity_health.IsEmpty());
                if (!sample.surviving_entity_health.IsEmpty()) {
                    TestRunner->TestEqual(TEXT("Destroyed defence objective reports zero health"),
                                          sample.surviving_entity_health[0],
                                          0);
                }
                break;
            }
            default: {
                checkNoEntry();
                break;
            }
        }
    }

    TEST_METHOD(SurviveTime)
    { setup_scenario(EScenario::SurviveTime); }

    TEST_METHOD(KillEnemies)
    { setup_scenario(EScenario::KillEnemies); }

    TEST_METHOD(KillEnemiesWithinTime)
    { setup_scenario(EScenario::KillEnemiesWithinTime); }

    TEST_METHOD(DefenceObjective)
    { setup_scenario(EScenario::DefenceObjective); }
};
