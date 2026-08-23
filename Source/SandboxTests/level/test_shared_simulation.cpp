#include "test_batch_orchestrator_reset_scenario.h"
#include "test_batch_orchestrator_setup_scenario.h"
#include "test_capital_ship_proxy_scenario.h"
#include "test_fighters_standby_transition_scenario.h"
#include "test_hud_manager_scenario.h"
#include "test_mission_manager_scenario.h"
#include "test_player_ship_death_scenario.h"
#include "test_spatial_query_line_of_sight_scenario.h"
#include "test_turret_line_of_sight_blocking_scenario.h"
#include "test_turret_search_requires_line_of_sight_scenario.h"

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
    { level_setup.teardown(); }
  private:
    template <typename T, typename... Args>
    void run_scenario(Args && ... args) {
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
        scenario = MakeUnique<T>(*context, Forward<Args>(args)...);
        check(scenario);
        scenario->run();
    }
  public:
    TEST_METHOD(Orchestrator_SpawnMissingActors)
    {
        run_scenario<ml::FTestBatchOrchestratorSetupScenario>(
            ml::EOrchestratorSetupScenario::SpawnMissingActors);
    }

    TEST_METHOD(Orchestrator_SimulationClockConversions)
    {
        run_scenario<ml::FTestBatchOrchestratorSetupScenario>(
            ml::EOrchestratorSetupScenario::SimulationClockConversions);
    }

    TEST_METHOD(Orchestrator_ResetForNewLevel)
    { run_scenario<ml::FTestBatchOrchestratorResetScenario>(); }

    TEST_METHOD(CapitalShipProxy_HealthOverridesConfig)
    { run_scenario<ml::FTestCapitalShipProxyScenario>(); }

    TEST_METHOD(Fighters_StandbyTransition)
    { run_scenario<ml::FFightersStandbyTransitionScenario>(); }

    TEST_METHOD(HUD_InitialCachesPopulateWithoutHUD)
    {
        run_scenario<ml::FTestHUDManagerScenario>(
            ml::EHUDManagerScenario::InitialCachesPopulateWithoutHUD);
    }

    TEST_METHOD(HUD_EntityCountPollingContinuesWithoutHUD)
    {
        run_scenario<ml::FTestHUDManagerScenario>(
            ml::EHUDManagerScenario::EntityCountPollingContinuesWithoutHUD);
    }

    TEST_METHOD(HUD_MissionAndDefenceDataUpdateWithoutHUD)
    {
        run_scenario<ml::FTestHUDManagerScenario>(
            ml::EHUDManagerScenario::MissionAndDefenceDataUpdateWithoutHUD);
    }

    TEST_METHOD(HUD_PlayerStateAndKillsUpdateWithoutHUD)
    {
        run_scenario<ml::FTestHUDManagerScenario>(
            ml::EHUDManagerScenario::PlayerStateAndKillsUpdateWithoutHUD);
    }

    TEST_METHOD(HUD_MissionTimeUsesSimulationClockWithoutHUD)
    {
        run_scenario<ml::FTestHUDManagerScenario>(
            ml::EHUDManagerScenario::MissionTimeUsesSimulationClockWithoutHUD);
    }

    TEST_METHOD(HUD_LateRegistrationSynchronisesAndUnregisters)
    {
        run_scenario<ml::FTestHUDManagerScenario>(
            ml::EHUDManagerScenario::LateHUDRegistrationSynchronisesAndUnregisters);
    }

    TEST_METHOD(Mission_SurviveTime)
    { run_scenario<ml::FTestMissionManagerScenario>(ml::EMissionManagerScenario::SurviveTime); }

    TEST_METHOD(Mission_KillEnemies)
    { run_scenario<ml::FTestMissionManagerScenario>(ml::EMissionManagerScenario::KillEnemies); }

    TEST_METHOD(Mission_KillEnemiesWithinTime)
    {
        run_scenario<ml::FTestMissionManagerScenario>(
            ml::EMissionManagerScenario::KillEnemiesWithinTime);
    }

    TEST_METHOD(Mission_DefenceObjective)
    {
        run_scenario<ml::FTestMissionManagerScenario>(
            ml::EMissionManagerScenario::DefenceObjective);
    }

    TEST_METHOD(PlayerShip_LethalDamageDestroysPlayerShip)
    { run_scenario<ml::FTestPlayerShipDeathScenario>(); }

    TEST_METHOD(SpatialQuery_ResolvesLineOfSightBatches)
    { run_scenario<ml::FSpatialQueryLineOfSightScenario>(); }

    TEST_METHOD(Turrets_LineOfSightBlocking)
    { run_scenario<ml::FTurretLineOfSightBlockingScenario>(); }

    TEST_METHOD(Turrets_SearchRequiresLineOfSight)
    { run_scenario<ml::FTurretSearchRequiresLineOfSightScenario>(); }
};
