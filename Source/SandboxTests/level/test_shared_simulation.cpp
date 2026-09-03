#include "test_batch_orchestrator_reset_scenario.h"
#include "test_batch_orchestrator_setup_scenario.h"
#include "test_capital_command_fighters_scenario.h"
#include "test_capital_fighter_handles_scenario.h"
#include "test_capital_ship_proxy_scenario.h"
#include "test_collision_uniform_grid_scenario.h"
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

#define SHARED_SIMULATION_TEST(METHOD_NAME, SCENARIO_TYPE, ...) \
    TEST_METHOD(METHOD_NAME)                                    \
    { run_scenario<SCENARIO_TYPE>(__VA_ARGS__); }

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
    SHARED_SIMULATION_TEST(Orchestrator_SpawnMissingActors,
                           ml::FTestBatchOrchestratorSetupScenario,
                           ml::EOrchestratorSetupScenario::SpawnMissingActors)
    SHARED_SIMULATION_TEST(Orchestrator_SimulationClockConversions,
                           ml::FTestBatchOrchestratorSetupScenario,
                           ml::EOrchestratorSetupScenario::SimulationClockConversions)
    SHARED_SIMULATION_TEST(Orchestrator_LevelTelemetry,
                           ml::FTestBatchOrchestratorSetupScenario,
                           ml::EOrchestratorSetupScenario::LevelTelemetry)
    SHARED_SIMULATION_TEST(Orchestrator_ResetForNewLevel, ml::FTestBatchOrchestratorResetScenario)

    SHARED_SIMULATION_TEST(CapitalShipProxy_HealthOverridesConfig,
                           ml::FTestCapitalShipProxyScenario)
    SHARED_SIMULATION_TEST(CapitalCommandFighters_RetargetAfterKills,
                           ml::FCapitalCommandFightersScenario)
    SHARED_SIMULATION_TEST(CapitalFighterHandles_KillFightersOnly,
                           ml::FCapitalFighterHandlesScenario,
                           ml::ECapitalFighterHandlesScenario::KillFightersOnly)
    SHARED_SIMULATION_TEST(CapitalFighterHandles_KillCapital,
                           ml::FCapitalFighterHandlesScenario,
                           ml::ECapitalFighterHandlesScenario::KillCapital)
    SHARED_SIMULATION_TEST(CapitalFighterHandles_All,
                           ml::FCapitalFighterHandlesScenario,
                           ml::ECapitalFighterHandlesScenario::All)

    SHARED_SIMULATION_TEST(Fighters_LineOfSightFailureHandling, ml::FFighterLosFailureScenario)
    SHARED_SIMULATION_TEST(EntityInterface_ConvertsProxiesAndResolvesTargets,
                           ml::FEntityInterfaceScenario)
    SHARED_SIMULATION_TEST(EntityRegistry_CountsTeams,
                           ml::FEntityRegistryScenario,
                           ml::EEntityRegistryScenario::TeamCounts)
    SHARED_SIMULATION_TEST(EntityRegistry_OnePlayerKill,
                           ml::FEntityRegistryScenario,
                           ml::EEntityRegistryScenario::OnePlayerKill)
    SHARED_SIMULATION_TEST(EntityRegistry_TwoPlayerKills,
                           ml::FEntityRegistryScenario,
                           ml::EEntityRegistryScenario::TwoPlayerKills)
    SHARED_SIMULATION_TEST(Fighters_StandbyTransition, ml::FFightersStandbyTransitionScenario)
    SHARED_SIMULATION_TEST(Fighters_InterceptCapital, ml::FFightersInterceptCapitalScenario)
    SHARED_SIMULATION_TEST(Fighters_AttackCapital, ml::FFighterAttackScenario)

    SHARED_SIMULATION_TEST(HUD_InitialCachesPopulateWithoutHUD,
                           ml::FTestHUDManagerScenario,
                           ml::EHUDManagerScenario::InitialCachesPopulateWithoutHUD)
    SHARED_SIMULATION_TEST(HUD_EntityCountPollingContinuesWithoutHUD,
                           ml::FTestHUDManagerScenario,
                           ml::EHUDManagerScenario::EntityCountPollingContinuesWithoutHUD)
    SHARED_SIMULATION_TEST(HUD_MissionAndDefenceDataUpdateWithoutHUD,
                           ml::FTestHUDManagerScenario,
                           ml::EHUDManagerScenario::MissionAndDefenceDataUpdateWithoutHUD)
    SHARED_SIMULATION_TEST(HUD_PlayerStateAndKillsUpdateWithoutHUD,
                           ml::FTestHUDManagerScenario,
                           ml::EHUDManagerScenario::PlayerStateAndKillsUpdateWithoutHUD)
    SHARED_SIMULATION_TEST(HUD_MissionTimeUsesSimulationClockWithoutHUD,
                           ml::FTestHUDManagerScenario,
                           ml::EHUDManagerScenario::MissionTimeUsesSimulationClockWithoutHUD)
    SHARED_SIMULATION_TEST(HUD_LateRegistrationSynchronisesAndUnregisters,
                           ml::FTestHUDManagerScenario,
                           ml::EHUDManagerScenario::LateHUDRegistrationSynchronisesAndUnregisters)

    SHARED_SIMULATION_TEST(Orchestrator_FixedStepPauseResumeAndCatchUp,
                           ml::FSimulationCoreRegressionScenario,
                           ml::ESimulationCoreRegressionScenario::FixedTickLifecycle)
    SHARED_SIMULATION_TEST(Entities_NonLethalThenLethalDamageCleansUpAtomically,
                           ml::FSimulationCoreRegressionScenario,
                           ml::ESimulationCoreRegressionScenario::DamageLifecycle)

    SHARED_SIMULATION_TEST(Mission_SurviveTime,
                           ml::FTestMissionManagerScenario,
                           ml::EMissionManagerScenario::SurviveTime)
    SHARED_SIMULATION_TEST(Mission_KillEnemies,
                           ml::FTestMissionManagerScenario,
                           ml::EMissionManagerScenario::KillEnemies)
    SHARED_SIMULATION_TEST(Mission_KillEnemiesWithinTime,
                           ml::FTestMissionManagerScenario,
                           ml::EMissionManagerScenario::KillEnemiesWithinTime)
    SHARED_SIMULATION_TEST(Mission_DefenceObjective,
                           ml::FTestMissionManagerScenario,
                           ml::EMissionManagerScenario::DefenceObjective)
    SHARED_SIMULATION_TEST(Mission_RequiredKillsObjective,
                           ml::FTestMissionManagerScenario,
                           ml::EMissionManagerScenario::RequiredKillsObjective)
    SHARED_SIMULATION_TEST(Mission_RequiredKillsTimeElapsed,
                           ml::FTestMissionManagerScenario,
                           ml::EMissionManagerScenario::RequiredKillsTimeElapsed)
    SHARED_SIMULATION_TEST(Mission_AutomaticKillTargetIncludesLastEnemy,
                           ml::FTestMissionManagerScenario,
                           ml::EMissionManagerScenario::AutomaticKillTarget)
    SHARED_SIMULATION_TEST(Mission_SuccessIsTerminal,
                           ml::FTestMissionManagerScenario,
                           ml::EMissionManagerScenario::SuccessIsTerminal)

    SHARED_SIMULATION_TEST(PlayerShip_LethalDamageDestroysPlayerShip,
                           ml::FTestPlayerShipDeathScenario)
    SHARED_SIMULATION_TEST(PlayerShip_VersusCapital, ml::FPlayerShipVsCapitalScenario)

    SHARED_SIMULATION_TEST(Lasers_QueuedSpawnHitsOnLaterTick,
                           ml::FLaserLifecycleScenario,
                           ml::ELaserLifecycleScenario::Hit)
    SHARED_SIMULATION_TEST(Lasers_SimultaneousHitsCauseOneDeath,
                           ml::FLaserLifecycleScenario,
                           ml::ELaserLifecycleScenario::SimultaneousLethalHits)
    SHARED_SIMULATION_TEST(Lasers_MissExpiresWithoutDamage,
                           ml::FLaserLifecycleScenario,
                           ml::ELaserLifecycleScenario::Miss)
    SHARED_SIMULATION_TEST(Lasers_WorldBlockerConsumesProjectileWithoutEntityDamage,
                           ml::FLaserLifecycleScenario,
                           ml::ELaserLifecycleScenario::WorldBlocker)

    SHARED_SIMULATION_TEST(SpatialQuery_ResolvesLineOfSightBatches,
                           ml::FSpatialQueryLineOfSightScenario)
    SHARED_SIMULATION_TEST(SpatialQuery_EmptyBatchesAndWorld, ml::FSpatialQueryEmptyScenario)
    SHARED_SIMULATION_TEST(SpatialQuery_TeamAndInclusiveRadiusFiltering,
                           ml::FSpatialQueryRangeScenario)
    SHARED_SIMULATION_TEST(SpatialQuery_ResolvesHitBatches, ml::FSpatialQueryResolutionScenario)
    SHARED_SIMULATION_TEST(Collision_UniformGridContainsAllEntityTypes,
                           ml::FCollisionUniformGridScenario)
    SHARED_SIMULATION_TEST(Collision_UniformGridTraceHitsAndMisses,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::HitsAndMisses)
    SHARED_SIMULATION_TEST(Collision_UniformGridTraceStopsAtEndpoint,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::StopsAtEndpoint)
    SHARED_SIMULATION_TEST(Collision_UniformGridTraceReturnsNearestHit,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::ReturnsNearestHit)
    SHARED_SIMULATION_TEST(Collision_UniformGridTraceHandlesZeroLengthTraces,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::HandlesZeroLengthTraces)
    SHARED_SIMULATION_TEST(Collision_UniformGridTraceIncludesNegativeEndpointBoundary,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::IncludesNegativeEndpointBoundary)
    SHARED_SIMULATION_TEST(Collision_UniformGridTraceAppliesAABBCentre,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::AppliesAABBCentre)
    SHARED_SIMULATION_TEST(Collision_UniformGridTraceAxisParallelAndOrigin,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::AxisParallelAndOrigin)
    SHARED_SIMULATION_TEST(Collision_UniformGridTraceSurfaceContacts,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::SurfaceContacts)
    SHARED_SIMULATION_TEST(Collision_UniformGridTraceGridBoundaryTraversal,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::GridBoundaryTraversal)
    SHARED_SIMULATION_TEST(Collision_UniformGridTraceShortAndNearParallelSegments,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::ShortAndNearParallelSegments)
    SHARED_SIMULATION_TEST(Collision_UniformGridTraceClipsToGridBounds,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::ClipsToGridBounds)
    SHARED_SIMULATION_TEST(Collision_UniformGridTraceDegenerateAABBs,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::DegenerateAABBs)
    SHARED_SIMULATION_TEST(Collision_UniformGridTraceCrossCellNearestHit,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::CrossCellNearestHit)
    SHARED_SIMULATION_TEST(Collision_UniformGridTraceVariedGridGeometry,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::VariedGridGeometry)
    SHARED_SIMULATION_TEST(Collision_UniformGridTraceBoundaryPrecision,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::BoundaryPrecision)
    SHARED_SIMULATION_TEST(Collision_UniformGridRebuildLifecycle,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::RebuildLifecycle)
    SHARED_SIMULATION_TEST(Collision_UniformGridTraceDeterministicReferenceSweep,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::DeterministicReferenceSweep)
    SHARED_SIMULATION_TEST(Collision_UniformGridTraceInvarianceProperties,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::InvarianceProperties)
    SHARED_SIMULATION_TEST(Collision_UniformGridTraceEmptyBatchesAndOutputReuse,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::EmptyBatchesAndOutputReuse)
    SHARED_SIMULATION_TEST(Collision_UniformGridDenseAndWideAABBs,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::DenseAndWideAABBs)
    SHARED_SIMULATION_TEST(Collision_UniformGridTraceProductionScale,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::ProductionScale)

    SHARED_SIMULATION_TEST(Turrets_LineOfSightBlocking, ml::FTurretLineOfSightBlockingScenario)
    SHARED_SIMULATION_TEST(
        Turrets_KillEnemy, ml::FTurretCombatScenario, ml::ETurretCombatScenario::KillEnemy)
    SHARED_SIMULATION_TEST(
        Turrets_ZeroDamage, ml::FTurretCombatScenario, ml::ETurretCombatScenario::ZeroDamage)
    SHARED_SIMULATION_TEST(Turrets_SearchRequiresLineOfSight,
                           ml::FTurretSearchRequiresLineOfSightScenario)
    SHARED_SIMULATION_TEST(Turrets_NoOtherEntityRemainsIdle,
                           ml::FTurretAcquisitionRegressionScenario,
                           ml::ETurretAcquisitionRegressionScenario::NoOtherEntity)
    SHARED_SIMULATION_TEST(Turrets_FriendlyOnlyRemainsIdle,
                           ml::FTurretAcquisitionRegressionScenario,
                           ml::ETurretAcquisitionRegressionScenario::FriendlyOnly)
    SHARED_SIMULATION_TEST(Turrets_EnemyOutsideDetectionRadiusRemainsIdle,
                           ml::FTurretAcquisitionRegressionScenario,
                           ml::ETurretAcquisitionRegressionScenario::EnemyOutsideRadius)
};

#undef SHARED_SIMULATION_TEST
