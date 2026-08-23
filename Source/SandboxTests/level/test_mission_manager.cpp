#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestSimulationDriver.h>

#include <SandboxTests/support/level_checks.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include "simulation_test_scenarios.h"

#include <SandboxCore/time_series_data.h>

#include <Sandbox/batch_game/SimulationConfig.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestMissionManager.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>

#include <SandboxCoreEngine/actor_utils.h>

#include <Engine/World.h>
#include <Misc/Optional.h>

namespace ml {
class FTestMissionManagerScenario final : public FSimulationTestScenario {
    using ThisClass = FTestMissionManagerScenario;
    using EScenario = EMissionManagerScenario;

    struct FSimulationSample {
        ETestMissionState mission_state{ETestMissionState::NotStarted};
        ETestMissionFailReason mission_fail_reason{ETestMissionFailReason::None};
        int32 mission_kills{0};
        TArray<int32> surviving_entity_health;
    };

    static constexpr float short_mission_time{0.1f};
    static constexpr float long_mission_time{10.f};
    inline static FTimespan const timeout{0, 0, 2};

    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};
    EScenario scenario;
    ml::TimeSeriesData<FSimulationSample> samples;

  public:
    FTestMissionManagerScenario(FSimulationTestContext& context, EScenario const new_scenario)
        : FSimulationTestScenario{context}, scenario{new_scenario} {
        test_driver.Reset();
        samples = {};
    }

    void tear_down() override {
        if (test_driver.IsSet()) {
            test_driver->orchestrator.clear_end_tick_test_hook();
        }
    }

  private:
    static void configure_level(UWorld & world,
                                UTestSimulationConfig const& config,
                                ml::FSoftTestAssertions& checks,
                                EScenario const scenario) {
        auto* const first_capital{ml::spawn_capital_proxy(
            world, config, checks, FName{TEXT("hero_capital")}, FVector{-2000.f, 0.f, 0.f})};
        if (!IsValid(first_capital)) {
            return;
        }
        switch (scenario) {
            case EScenario::SurviveTime: {
                break;
            }
            case EScenario::KillEnemies: {
                ml::spawn_capital_proxy(
                    world, config, checks, FName{TEXT("enemy_capital")}, FVector{2000.f, 0.f, 0.f});
                break;
            }
            case EScenario::KillEnemiesWithinTime: {
                ml::spawn_capital_proxy(
                    world, config, checks, FName{TEXT("enemy_capital")}, FVector{2000.f, 0.f, 0.f});
                break;
            }
            case EScenario::DefenceObjective: {
                break;
            }
            default: {
                checkNoEntry();
                break;
            }
        }
    }

    static void configure_mission_manager(
        UWorld & world, ATestBatchOrchestrator & orchestrator, EScenario const scenario) {
        auto* const first_capital{ml::get_first_actor<ATestCapitalShipProxy>(world)};
        check(first_capital);

        auto& manager{orchestrator.get_mission_manager()};
        manager.set_save_mission_results(false);

        switch (scenario) {
            case EScenario::SurviveTime: {
                manager.set_mission_mode(ETestMissionMode::SurviveTime);
                manager.set_target_time(short_mission_time);
                manager.add_entity_that_must_survive(*first_capital);
                break;
            }
            case EScenario::KillEnemies: {
                manager.set_mission_mode(ETestMissionMode::KillEnemies);
                manager.set_kill_target(1);
                manager.add_hero_entity(*first_capital);
                break;
            }
            case EScenario::KillEnemiesWithinTime: {
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
    }

    void setup_scenario(EScenario const new_scenario) {
        scenario = new_scenario;
        TestCommandBuilder.Do([this, new_scenario] {
            auto& world{context_.world};
            configure_level(world, context_.config, checks, new_scenario);
            auto* const orchestrator{&context_.orchestrator};
            if (checks.is_valid(orchestrator, TEXT("Orchestrator is available"))) {
                configure_mission_manager(world, *orchestrator, new_scenario);
            }
        });

        TestCommandBuilder.Do([this] { start_scenario(); })
            .Until([this] { return mission_has_ended(); }, timeout)
            .Then([this] { check_scenario_result(); });
    }

    void start_scenario() {
        auto& world{context_.world};
        test_driver = ml::TestSimulationDriver::from_world(world);

        auto* const manager{&test_driver->orchestrator.get_mission_manager()};

        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));
        if (scenario == EScenario::KillEnemies) {
            test_driver->timeline.then_after(0.01, [this, manager] { queue_enemy_kill(*manager); });
        } else if (scenario == EScenario::DefenceObjective) {
            test_driver->timeline.then_after(
                0.01, [this, manager] { queue_defended_entity_kill(*manager); });
        }
        test_driver->orchestrator.start_simulation();

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
    }

    void on_end_tick(ATestBatchOrchestrator&) {
        auto const& manager{test_driver->orchestrator.get_mission_manager()};

        FSimulationSample sample{};
        sample.mission_state = manager.get_mission_state();
        sample.mission_fail_reason = manager.get_mission_fail_reason();
        sample.mission_kills = manager.get_mission_kills();
        for (auto const& health : manager.get_entity_health_that_must_survive()) {
            sample.surviving_entity_health.Add(health.health);
        }

        samples.add(test_driver->get_time(), MoveTemp(sample));
        test_driver->timeline.tick(test_driver->get_time());
    }

    void queue_enemy_kill(FTestMissionManager const& manager) {
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

    void queue_defended_entity_kill(FTestMissionManager const& manager) {
        auto const defended_handles{manager.get_entity_handles_that_must_survive()};
        check(defended_handles.Num() == 1);

        TArray<FRegistryEntityHandle> const targets{defended_handles[0]};
        auto const damage{test_driver->get_capital_ships().get_health(defended_handles[0])};
        test_driver->queue_damage(targets, damage);
    }

    auto mission_has_ended() const -> bool {
        auto const& manager{test_driver->orchestrator.get_mission_manager()};
        auto const state{manager.get_mission_state()};
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

  public:
    void run() override { setup_scenario(scenario); }
};

auto make_mission_manager_scenario(FSimulationTestContext& context,
                                   EMissionManagerScenario const scenario)
    -> TUniquePtr<FSimulationTestScenario> {
    return MakeUnique<FTestMissionManagerScenario>(context, scenario);
}
}
