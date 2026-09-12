#include <SandboxTests/level/test_capital_command_fighters.h>
#include <SandboxTests/level/test_capital_fighter_handles.h>
#include <SandboxTests/level/test_collision_uniform_grid.h>
#include <SandboxTests/level/test_entity_registry.h>
#include <SandboxTests/level/test_fighter_attack.h>
#include <SandboxTests/level/test_fighter_los_failure.h>
#include <SandboxTests/level/test_fighters_intercept_capital.h>
#include <SandboxTests/level/test_fighters_standby_transition.h>
#include <SandboxTests/level/test_laser_lifecycle.h>
#include <SandboxTests/level/test_mission_manager.h>
#include <SandboxTests/level/test_player_ship_vs_capital.h>
#include <SandboxTests/level/test_simulation_core_regressions.h>
#include <SandboxTests/level/test_spatial_query_empty.h>
#include <SandboxTests/level/test_spatial_query_manager.h>
#include <SandboxTests/level/test_turret_acquisition_regressions.h>
#include <SandboxTests/level/test_turrets_kill_one.h>
#include "../level/test_hud_manager_scenario.h"

#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/SoftTestAssertions.h>

#include <CQTest.h>

TEST_CLASS(WorldlessSpaceGameSimulation, "Sandbox.UnitTests")
{
    ml::FSoftTestAssertions checks{};
    USpaceGameLevelConfig const* config{};

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        config = ml::get_default_level_config(checks);
    }
  private:
    template <typename Runner, typename... Args>
    void run(Runner && runner, Args && ... args) {
        if (config != nullptr) {
            runner(*TestRunner, checks, *config, Forward<Args>(args)...);
        }
        ASSERT_THAT(IsTrue(checks.all_passed, TEXT("all_passed")));
    }
  public:
    TEST_METHOD(CapitalCommandFighters_RetargetAfterKills)
    { run(ml::run_worldless_capital_command_fighters); }
    TEST_METHOD(CapitalFighterHandles_KillFightersOnly)
    {
        run(ml::run_worldless_capital_fighter_handles,
            ml::ECapitalFighterHandlesScenario::KillFightersOnly);
    }
    TEST_METHOD(CapitalFighterHandles_KillCapital)
    {
        run(ml::run_worldless_capital_fighter_handles,
            ml::ECapitalFighterHandlesScenario::KillCapital);
    }
    TEST_METHOD(CapitalFighterHandles_All)
    { run(ml::run_worldless_capital_fighter_handles, ml::ECapitalFighterHandlesScenario::All); }
    TEST_METHOD(Capitals_SimultaneousReassignment)
    { run(ml::run_worldless_simultaneous_capital_reassignment); }

    TEST_METHOD(Fighters_LineOfSightFailureHandling)
    { run(ml::run_worldless_fighter_los_failure); }
    TEST_METHOD(Fighters_StandbyTransition)
    { run(ml::run_worldless_fighters_standby_transition); }
    TEST_METHOD(Fighters_InterceptCapital)
    { run(ml::run_worldless_fighters_intercept_capital); }
    TEST_METHOD(Fighters_AttackCapital)
    { run(ml::run_worldless_fighter_attack); }
    TEST_METHOD(Fighters_ObstacleAvoidance)
    { run(ml::run_worldless_fighter_obstacle_avoidance); }
    TEST_METHOD(Fighters_NavigationCapitalObstruction)
    { run(ml::run_worldless_fighter_capital_obstruction); }
    TEST_METHOD(Fighters_NavigationClearPath)
    { run(ml::run_worldless_fighter_clear_navigation); }
    TEST_METHOD(Fighters_NavigationSeparation)
    { run(ml::run_worldless_fighter_separation); }
    TEST_METHOD(Fighters_NavigationDenseDeterminism)
    { run(ml::run_worldless_fighter_dense_determinism); }
    TEST_METHOD(Fighters_NavigationLargeCluster)
    { run(ml::run_worldless_fighter_large_cluster); }
    TEST_METHOD(Fighters_NavigationHardAvoidanceAuthority)
    { run(ml::run_worldless_fighter_hard_avoidance_authority); }
    TEST_METHOD(Fighters_NavigationFrequency)
    { run(ml::run_worldless_fighter_navigation_frequency); }

    TEST_METHOD(EntityRegistry_CountsTeams)
    { run(ml::run_worldless_entity_registry_scenario, ml::EEntityRegistryScenario::TeamCounts); }
    TEST_METHOD(EntityRegistry_OnePlayerKill)
    { run(ml::run_worldless_entity_registry_scenario, ml::EEntityRegistryScenario::OnePlayerKill); }
    TEST_METHOD(EntityRegistry_TwoPlayerKills)
    {
        run(ml::run_worldless_entity_registry_scenario,
            ml::EEntityRegistryScenario::TwoPlayerKills);
    }

    TEST_METHOD(HUD_InitialCachesPopulateWithoutHUD)
    {
        run(ml::run_worldless_hud_manager_scenario,
            ml::EHUDManagerScenario::InitialCachesPopulateWithoutHUD);
    }
    TEST_METHOD(HUD_EntityCountPollingContinuesWithoutHUD)
    {
        run(ml::run_worldless_hud_manager_scenario,
            ml::EHUDManagerScenario::EntityCountPollingContinuesWithoutHUD);
    }
    TEST_METHOD(HUD_MissionAndDefenceDataUpdateWithoutHUD)
    {
        run(ml::run_worldless_hud_manager_scenario,
            ml::EHUDManagerScenario::MissionAndDefenceDataUpdateWithoutHUD);
    }
    TEST_METHOD(HUD_PlayerStateAndKillsUpdateWithoutHUD)
    {
        run(ml::run_worldless_hud_manager_scenario,
            ml::EHUDManagerScenario::PlayerStateAndKillsUpdateWithoutHUD);
    }
    TEST_METHOD(HUD_MissionTimeUsesSimulationClockWithoutHUD)
    {
        run(ml::run_worldless_hud_manager_scenario,
            ml::EHUDManagerScenario::MissionTimeUsesSimulationClockWithoutHUD);
    }

    TEST_METHOD(Orchestrator_FixedStepPauseResumeAndCatchUp)
    {
        run(ml::run_worldless_simulation_core_regression,
            ml::ESimulationCoreRegressionScenario::FixedTickLifecycle);
    }
    TEST_METHOD(Entities_NonLethalThenLethalDamageCleansUpAtomically)
    {
        run(ml::run_worldless_simulation_core_regression,
            ml::ESimulationCoreRegressionScenario::DamageLifecycle);
    }
    TEST_METHOD(Entities_CollisionOverlapDamagesAndKills)
    { run(ml::run_worldless_collision_damage); }

    void run_mission(ml::EMissionManagerScenario const scenario) {
        if (config != nullptr) {
            ml::run_worldless_mission_manager_scenario(*TestRunner, *config, scenario);
        }
        ASSERT_THAT(IsTrue(checks.all_passed, TEXT("all_passed")));
    }

    TEST_METHOD(Mission_SurviveTime)
    { run_mission(ml::EMissionManagerScenario::SurviveTime); }
    TEST_METHOD(Mission_KillEnemies)
    { run_mission(ml::EMissionManagerScenario::KillEnemies); }
    TEST_METHOD(Mission_KillEnemiesWithinTime)
    { run_mission(ml::EMissionManagerScenario::KillEnemiesWithinTime); }
    TEST_METHOD(Mission_DefenceObjective)
    { run_mission(ml::EMissionManagerScenario::DefenceObjective); }
    TEST_METHOD(Mission_RequiredKillsObjective)
    { run_mission(ml::EMissionManagerScenario::RequiredKillsObjective); }
    TEST_METHOD(Mission_RequiredKillsTimeElapsed)
    { run_mission(ml::EMissionManagerScenario::RequiredKillsTimeElapsed); }
    TEST_METHOD(Mission_AutomaticKillTargetIncludesLastEnemy)
    { run_mission(ml::EMissionManagerScenario::AutomaticKillTarget); }
    TEST_METHOD(Mission_SuccessIsTerminal)
    { run_mission(ml::EMissionManagerScenario::SuccessIsTerminal); }
    TEST_METHOD(Mission_ExplicitCompletionIsLatched)
    { run_mission(ml::EMissionManagerScenario::ExplicitCompletionIsLatched); }

    TEST_METHOD(PlayerShip_VersusCapital)
    { run(ml::run_worldless_player_ship_vs_capital); }

    TEST_METHOD(Lasers_QueuedSpawnHitsOnLaterTick)
    { run(ml::run_worldless_laser_lifecycle, ml::ELaserLifecycleScenario::Hit); }
    TEST_METHOD(Lasers_SimultaneousHitsCauseOneDeath)
    { run(ml::run_worldless_laser_lifecycle, ml::ELaserLifecycleScenario::SimultaneousLethalHits); }
    TEST_METHOD(Lasers_MissExpiresWithoutDamage)
    { run(ml::run_worldless_laser_lifecycle, ml::ELaserLifecycleScenario::Miss); }
    TEST_METHOD(Lasers_WorldBlockerConsumesProjectileWithoutEntityDamage)
    { run(ml::run_worldless_laser_lifecycle, ml::ELaserLifecycleScenario::WorldBlocker); }

    TEST_METHOD(SpatialQuery_ResolvesLineOfSightBatches)
    { run(ml::run_worldless_spatial_query_line_of_sight); }
    TEST_METHOD(SpatialQuery_EmptyBatchesAndWorld)
    { run(ml::run_worldless_spatial_query_empty); }
    TEST_METHOD(SpatialQuery_TeamAndInclusiveRadiusFiltering)
    { run(ml::run_worldless_spatial_query_range); }

    TEST_METHOD(Collision_UniformGridTraceHitsAndMisses)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::HitsAndMisses);
    }
    TEST_METHOD(Collision_UniformGridContainsAllEntityTypes)
    { run(ml::run_worldless_collision_uniform_grid_membership); }
    TEST_METHOD(Collision_UniformGridTraceStopsAtEndpoint)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::StopsAtEndpoint);
    }
    TEST_METHOD(Collision_UniformGridTraceReturnsNearestHit)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::ReturnsNearestHit);
    }
    TEST_METHOD(Collision_UniformGridTraceHandlesZeroLengthTraces)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::HandlesZeroLengthTraces);
    }
    TEST_METHOD(Collision_UniformGridTraceIncludesNegativeEndpointBoundary)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::IncludesNegativeEndpointBoundary);
    }
    TEST_METHOD(Collision_UniformGridTraceAppliesAABBCentre)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::AppliesAABBCentre);
    }
    TEST_METHOD(Collision_UniformGridTraceAxisParallelAndOrigin)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::AxisParallelAndOrigin);
    }
    TEST_METHOD(Collision_UniformGridTraceSurfaceContacts)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::SurfaceContacts);
    }
    TEST_METHOD(Collision_UniformGridTraceGridBoundaryTraversal)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::GridBoundaryTraversal);
    }
    TEST_METHOD(Collision_UniformGridTraceShortAndNearParallelSegments)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::ShortAndNearParallelSegments);
    }
    TEST_METHOD(Collision_UniformGridTraceClipsToGridBounds)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::ClipsToGridBounds);
    }
    TEST_METHOD(Collision_UniformGridTraceDegenerateAABBs)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::DegenerateAABBs);
    }
    TEST_METHOD(Collision_UniformGridTraceCrossCellNearestHit)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::CrossCellNearestHit);
    }
    TEST_METHOD(Collision_UniformGridTraceVariedGridGeometry)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::VariedGridGeometry);
    }
    TEST_METHOD(Collision_UniformGridTraceBoundaryPrecision)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::BoundaryPrecision);
    }
    TEST_METHOD(Collision_UniformGridRebuildLifecycle)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::RebuildLifecycle);
    }
    TEST_METHOD(Collision_UniformGridTraceDeterministicReferenceSweep)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::DeterministicReferenceSweep);
    }
    TEST_METHOD(Collision_UniformGridTraceInvarianceProperties)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::InvarianceProperties);
    }
    TEST_METHOD(Collision_UniformGridTraceEmptyBatchesAndOutputReuse)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::EmptyBatchesAndOutputReuse);
    }
    TEST_METHOD(Collision_UniformGridDenseAndWideAABBs)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::DenseAndWideAABBs);
    }
    TEST_METHOD(Collision_UniformGridTraceProductionScale)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::ProductionScale);
    }
    TEST_METHOD(Collision_UniformGridStaticGeometry)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::StaticGeometry);
    }
    TEST_METHOD(Collision_UniformGridStaticHarvesting)
    {
        run(ml::run_collision_uniform_grid_trace,
            ml::ECollisionUniformGridTraceScenario::StaticHarvesting);
    }

    TEST_METHOD(Turrets_LineOfSightBlocking)
    { run(ml::run_worldless_turret_line_of_sight_blocking); }
    TEST_METHOD(Turrets_KillEnemy)
    { run(ml::run_worldless_turret_combat, ml::ETurretCombatScenario::KillEnemy); }
    TEST_METHOD(Turrets_ZeroDamage)
    { run(ml::run_worldless_turret_combat, ml::ETurretCombatScenario::ZeroDamage); }
    TEST_METHOD(Turrets_SearchRequiresLineOfSight)
    { run(ml::run_worldless_turret_search_requires_line_of_sight); }
    TEST_METHOD(Turrets_NoOtherEntityRemainsIdle)
    {
        run(ml::run_worldless_turret_acquisition_regression,
            ml::ETurretAcquisitionRegressionScenario::NoOtherEntity);
    }
    TEST_METHOD(Turrets_FriendlyOnlyRemainsIdle)
    {
        run(ml::run_worldless_turret_acquisition_regression,
            ml::ETurretAcquisitionRegressionScenario::FriendlyOnly);
    }
    TEST_METHOD(Turrets_EnemyOutsideDetectionRadiusRemainsIdle)
    {
        run(ml::run_worldless_turret_acquisition_regression,
            ml::ETurretAcquisitionRegressionScenario::EnemyOutsideRadius);
    }
};
