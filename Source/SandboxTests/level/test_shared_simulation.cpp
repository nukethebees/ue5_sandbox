#include "simulation_test_scenarios.h"

#include <SandboxTests/support/test_setup.h>

#include <Sandbox/batch_game/TestBatchOrchestrator.h>

#include <CQTest.h>

TEST_CLASS(SharedSimulation, "Sandbox.LevelTests")
{
    inline static ml::FTestBatchOrchestratorLevelSetup level_setup{};

    ml::FSoftTestAssertions checks{};
    TUniquePtr<ml::FSimulationTestContext> context{nullptr};
    TUniquePtr<ml::FSimulationTestScenario> scenario{nullptr};

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        level_setup.begin_test(TestCommandBuilder, *TestRunner, checks);
    }

    AFTER_EACH()
    {
        if (scenario) {
            scenario->tear_down();
        }
        scenario.Reset();
        context.Reset();
        level_setup.end_test();
    }

    AFTER_ALL()
    {
        level_setup.teardown();
    }

  private:
    template <typename Factory, typename... Args>
    void run_scenario(Factory const factory, Args... args) {
        auto* const orchestrator{level_setup.get_orchestrator()};
        if (!checks.is_valid(orchestrator, TEXT("Shared simulation orchestrator is available"))) {
            return;
        }

        context = MakeUnique<ml::FSimulationTestContext>(ml::FSimulationTestContext{
            .orchestrator = *orchestrator,
            .automation_test = *TestRunner,
            .command_builder = TestCommandBuilder,
            .checks = checks,
            .config = level_setup.get_config(),
            .world = level_setup.get_world(),
            .level_construction_count = level_setup.get_construction_count(),
        });
        scenario = factory(*context, args...);
        check(scenario);
        scenario->run();
    }

  public:
    TEST_METHOD(Orchestrator_SpawnMissingActors)
    {
        run_scenario(ml::make_orchestrator_setup_scenario,
                     ml::EOrchestratorSetupScenario::SpawnMissingActors);
    }

    TEST_METHOD(Orchestrator_SimulationClockConversions)
    {
        run_scenario(ml::make_orchestrator_setup_scenario,
                     ml::EOrchestratorSetupScenario::SimulationClockConversions);
    }

    TEST_METHOD(Orchestrator_ResetForNewLevel)
    {
        run_scenario(ml::make_orchestrator_reset_scenario);
    }

    TEST_METHOD(CapitalShipProxy_HealthOverridesConfig)
    {
        run_scenario(ml::make_capital_ship_proxy_scenario);
    }

    TEST_METHOD(Fighters_StandbyTransition)
    {
        run_scenario(ml::make_fighters_standby_scenario);
    }

    TEST_METHOD(HUD_InitialCachesPopulateWithoutHUD)
    {
        run_scenario(ml::make_hud_manager_scenario,
                     ml::EHUDManagerScenario::InitialCachesPopulateWithoutHUD);
    }

    TEST_METHOD(HUD_EntityCountPollingContinuesWithoutHUD)
    {
        run_scenario(ml::make_hud_manager_scenario,
                     ml::EHUDManagerScenario::EntityCountPollingContinuesWithoutHUD);
    }

    TEST_METHOD(HUD_MissionAndDefenceDataUpdateWithoutHUD)
    {
        run_scenario(ml::make_hud_manager_scenario,
                     ml::EHUDManagerScenario::MissionAndDefenceDataUpdateWithoutHUD);
    }

    TEST_METHOD(HUD_PlayerStateAndKillsUpdateWithoutHUD)
    {
        run_scenario(ml::make_hud_manager_scenario,
                     ml::EHUDManagerScenario::PlayerStateAndKillsUpdateWithoutHUD);
    }

    TEST_METHOD(HUD_MissionTimeUsesSimulationClockWithoutHUD)
    {
        run_scenario(ml::make_hud_manager_scenario,
                     ml::EHUDManagerScenario::MissionTimeUsesSimulationClockWithoutHUD);
    }

    TEST_METHOD(HUD_LateRegistrationSynchronisesAndUnregisters)
    {
        run_scenario(ml::make_hud_manager_scenario,
                     ml::EHUDManagerScenario::LateHUDRegistrationSynchronisesAndUnregisters);
    }

    TEST_METHOD(Mission_SurviveTime)
    {
        run_scenario(ml::make_mission_manager_scenario,
                     ml::EMissionManagerScenario::SurviveTime);
    }

    TEST_METHOD(Mission_KillEnemies)
    {
        run_scenario(ml::make_mission_manager_scenario,
                     ml::EMissionManagerScenario::KillEnemies);
    }

    TEST_METHOD(Mission_KillEnemiesWithinTime)
    {
        run_scenario(ml::make_mission_manager_scenario,
                     ml::EMissionManagerScenario::KillEnemiesWithinTime);
    }

    TEST_METHOD(Mission_DefenceObjective)
    {
        run_scenario(ml::make_mission_manager_scenario,
                     ml::EMissionManagerScenario::DefenceObjective);
    }

    TEST_METHOD(PlayerShip_LethalDamageDestroysPlayerShip)
    {
        run_scenario(ml::make_player_ship_death_scenario);
    }

    TEST_METHOD(SpatialQuery_ResolvesLineOfSightBatches)
    {
        run_scenario(ml::make_spatial_query_line_of_sight_scenario);
    }

    TEST_METHOD(Turrets_LineOfSightBlocking)
    {
        run_scenario(ml::make_turret_line_of_sight_blocking_scenario);
    }

    TEST_METHOD(Turrets_SearchRequiresLineOfSight)
    {
        run_scenario(ml::make_turret_search_line_of_sight_scenario);
    }
};
