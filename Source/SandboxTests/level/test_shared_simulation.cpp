#include "test_batch_orchestrator_reset_scenario.h"
#include "test_batch_orchestrator_setup_scenario.h"
#include "test_capital_command_fighters_scenario.h"
#include "test_capital_fighter_handles_scenario.h"
#include "test_capital_ship_proxy_scenario.h"
#include "test_entity_interface_scenario.h"
#include "test_entity_registry_scenario.h"
#include "test_fighter_attack_scenario.h"
#include "test_fighter_los_failure_scenario.h"
#include "test_fighters_intercept_capital_scenario.h"
#include "test_fighters_standby_transition_scenario.h"
#include "test_hud_manager_scenario.h"
#include "test_laser_lifecycle_scenario.h"
#include "test_mission_manager_scenario.h"
#include "test_player_ship_death_scenario.h"
#include "test_player_ship_vs_capital_scenario.h"
#include "test_simulation_core_regressions_scenario.h"
#include "test_spatial_query_empty_scenario.h"
#include "test_spatial_query_line_of_sight_scenario.h"
#include "test_spatial_query_resolution_scenario.h"
#include "test_turret_acquisition_regressions_scenario.h"
#include "test_turret_combat_scenario.h"
#include "test_turret_line_of_sight_blocking_scenario.h"
#include "test_turret_search_requires_line_of_sight_scenario.h"

#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/simulation/TestBatchOrchestrator.h>

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

    TEST_METHOD(Orchestrator_LevelTelemetry)
    {
        run_scenario<ml::FTestBatchOrchestratorSetupScenario>(
            ml::EOrchestratorSetupScenario::LevelTelemetry);
    }

    TEST_METHOD(Orchestrator_ResetForNewLevel)
    { run_scenario<ml::FTestBatchOrchestratorResetScenario>(); }

    TEST_METHOD(Orchestrator_FixedStepPauseResumeAndCatchUp)
    {
        run_scenario<ml::FSimulationCoreRegressionScenario>(
            ml::ESimulationCoreRegressionScenario::FixedTickLifecycle);
    }

    TEST_METHOD(Entities_NonLethalThenLethalDamageCleansUpAtomically)
    {
        run_scenario<ml::FSimulationCoreRegressionScenario>(
            ml::ESimulationCoreRegressionScenario::DamageLifecycle);
    }

    TEST_METHOD(CapitalShipProxy_HealthOverridesConfig)
    { run_scenario<ml::FTestCapitalShipProxyScenario>(); }

    TEST_METHOD(CapitalCommandFighters_RetargetAfterKills)
    { run_scenario<ml::FCapitalCommandFightersScenario>(); }

    TEST_METHOD(CapitalFighterHandles_KillFightersOnly)
    {
        run_scenario<ml::FCapitalFighterHandlesScenario>(
            ml::ECapitalFighterHandlesScenario::KillFightersOnly);
    }

    TEST_METHOD(CapitalFighterHandles_KillCapital)
    {
        run_scenario<ml::FCapitalFighterHandlesScenario>(
            ml::ECapitalFighterHandlesScenario::KillCapital);
    }

    TEST_METHOD(CapitalFighterHandles_All)
    { run_scenario<ml::FCapitalFighterHandlesScenario>(ml::ECapitalFighterHandlesScenario::All); }

    TEST_METHOD(Fighters_LineOfSightFailureHandling)
    { run_scenario<ml::FFighterLosFailureScenario>(); }

    TEST_METHOD(EntityInterface_ConvertsProxiesAndResolvesTargets)
    { run_scenario<ml::FEntityInterfaceScenario>(); }

    TEST_METHOD(EntityRegistry_CountsTeams)
    { run_scenario<ml::FEntityRegistryScenario>(ml::EEntityRegistryScenario::TeamCounts); }

    TEST_METHOD(EntityRegistry_OnePlayerKill)
    { run_scenario<ml::FEntityRegistryScenario>(ml::EEntityRegistryScenario::OnePlayerKill); }

    TEST_METHOD(EntityRegistry_TwoPlayerKills)
    { run_scenario<ml::FEntityRegistryScenario>(ml::EEntityRegistryScenario::TwoPlayerKills); }

    TEST_METHOD(Fighters_StandbyTransition)
    { run_scenario<ml::FFightersStandbyTransitionScenario>(); }

    TEST_METHOD(Fighters_InterceptCapital)
    { run_scenario<ml::FFightersInterceptCapitalScenario>(); }

    TEST_METHOD(Fighters_AttackCapital)
    { run_scenario<ml::FFighterAttackScenario>(); }

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

    TEST_METHOD(Mission_RequiredKillsObjective)
    {
        run_scenario<ml::FTestMissionManagerScenario>(
            ml::EMissionManagerScenario::RequiredKillsObjective);
    }

    TEST_METHOD(Mission_RequiredKillsTimeElapsed)
    {
        run_scenario<ml::FTestMissionManagerScenario>(
            ml::EMissionManagerScenario::RequiredKillsTimeElapsed);
    }

    TEST_METHOD(Mission_AutomaticKillTargetIncludesLastEnemy)
    {
        run_scenario<ml::FTestMissionManagerScenario>(
            ml::EMissionManagerScenario::AutomaticKillTarget);
    }

    TEST_METHOD(Mission_SuccessIsTerminal)
    {
        run_scenario<ml::FTestMissionManagerScenario>(
            ml::EMissionManagerScenario::SuccessIsTerminal);
    }

    TEST_METHOD(PlayerShip_LethalDamageDestroysPlayerShip)
    { run_scenario<ml::FTestPlayerShipDeathScenario>(); }

    TEST_METHOD(PlayerShip_VersusCapital)
    { run_scenario<ml::FPlayerShipVsCapitalScenario>(); }

    TEST_METHOD(Lasers_QueuedSpawnHitsOnLaterTick)
    { run_scenario<ml::FLaserLifecycleScenario>(ml::ELaserLifecycleScenario::Hit); }

    TEST_METHOD(Lasers_SimultaneousHitsCauseOneDeath)
    {
        run_scenario<ml::FLaserLifecycleScenario>(
            ml::ELaserLifecycleScenario::SimultaneousLethalHits);
    }

    TEST_METHOD(Lasers_MissExpiresWithoutDamage)
    { run_scenario<ml::FLaserLifecycleScenario>(ml::ELaserLifecycleScenario::Miss); }

    TEST_METHOD(Lasers_WorldBlockerConsumesProjectileWithoutEntityDamage)
    { run_scenario<ml::FLaserLifecycleScenario>(ml::ELaserLifecycleScenario::WorldBlocker); }

    TEST_METHOD(SpatialQuery_ResolvesLineOfSightBatches)
    { run_scenario<ml::FSpatialQueryLineOfSightScenario>(); }

    TEST_METHOD(SpatialQuery_EmptyBatchesAndWorld)
    { run_scenario<ml::FSpatialQueryEmptyScenario>(); }

    TEST_METHOD(SpatialQuery_TeamAndInclusiveRadiusFiltering)
    { run_scenario<ml::FSpatialQueryRangeScenario>(); }

    TEST_METHOD(SpatialQuery_ResolvesHitBatches)
    { run_scenario<ml::FSpatialQueryResolutionScenario>(); }

    TEST_METHOD(Turrets_LineOfSightBlocking)
    { run_scenario<ml::FTurretLineOfSightBlockingScenario>(); }

    TEST_METHOD(Turrets_KillEnemy)
    { run_scenario<ml::FTurretCombatScenario>(ml::ETurretCombatScenario::KillEnemy); }

    TEST_METHOD(Turrets_ZeroDamage)
    { run_scenario<ml::FTurretCombatScenario>(ml::ETurretCombatScenario::ZeroDamage); }

    TEST_METHOD(Turrets_SearchRequiresLineOfSight)
    { run_scenario<ml::FTurretSearchRequiresLineOfSightScenario>(); }

    TEST_METHOD(Turrets_NoOtherEntityRemainsIdle)
    {
        run_scenario<ml::FTurretAcquisitionRegressionScenario>(
            ml::ETurretAcquisitionRegressionScenario::NoOtherEntity);
    }

    TEST_METHOD(Turrets_FriendlyOnlyRemainsIdle)
    {
        run_scenario<ml::FTurretAcquisitionRegressionScenario>(
            ml::ETurretAcquisitionRegressionScenario::FriendlyOnly);
    }

    TEST_METHOD(Turrets_EnemyOutsideDetectionRadiusRemainsIdle)
    {
        run_scenario<ml::FTurretAcquisitionRegressionScenario>(
            ml::ETurretAcquisitionRegressionScenario::EnemyOutsideRadius);
    }
};
