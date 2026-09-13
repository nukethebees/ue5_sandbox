#include "scenarios/test_capital_command_fighters.h"
#include "scenarios/test_capital_fighter_handles.h"
#include "scenarios/test_collision_uniform_grid.h"
#include "scenarios/test_entity_registry.h"
#include "scenarios/test_fighter_attack.h"
#include "scenarios/test_fighter_los_failure.h"
#include "scenarios/test_fighters_intercept_capital.h"
#include "scenarios/test_fighters_standby_transition.h"
#include "scenarios/test_laser_lifecycle.h"
#include "scenarios/test_mission_manager.h"
#include "scenarios/test_player_ship_vs_capital.h"
#include "scenarios/test_simulation_core_regressions.h"
#include "scenarios/test_spatial_query_empty.h"
#include "scenarios/test_spatial_query_manager.h"
#include "scenarios/test_turret_acquisition_regressions.h"
#include "scenarios/test_turrets_kill_one.h"
#include "support/simulation_test_support.h"

TEST(WorldlessSpaceGameSimulation, CapitalCommandFighters_RetargetAfterKills) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_capital_command_fighters(config);
}

TEST(WorldlessSpaceGameSimulation, CapitalFighterHandles_KillFightersOnly) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_worldless_capital_fighter_handles(config,
                                              ml::ECapitalFighterHandlesScenario::KillFightersOnly);
}

TEST(WorldlessSpaceGameSimulation, CapitalFighterHandles_KillCapital) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_worldless_capital_fighter_handles(config,
                                              ml::ECapitalFighterHandlesScenario::KillCapital);
}

TEST(WorldlessSpaceGameSimulation, CapitalFighterHandles_All) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_capital_fighter_handles(config, ml::ECapitalFighterHandlesScenario::All);
}

TEST(WorldlessSpaceGameSimulation, Capitals_SimultaneousReassignment) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_simultaneous_capital_reassignment(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_LineOfSightFailureHandling) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_fighter_los_failure(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_StandbyTransition) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_fighters_standby_transition(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_InterceptCapital) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_fighters_intercept_capital(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_AttackCapital) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_fighter_attack(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_ObstacleAvoidance) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_fighter_obstacle_avoidance(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_NavigationCapitalObstruction) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_fighter_capital_obstruction(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_NavigationClearPath) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_fighter_clear_navigation(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_NavigationSeparation) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_fighter_separation(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_NavigationDenseDeterminism) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_fighter_dense_determinism(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_NavigationLargeCluster) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_fighter_large_cluster(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_NavigationHardAvoidanceAuthority) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_fighter_hard_avoidance_authority(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_NavigationFrequency) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_fighter_navigation_frequency(config);
}

TEST(WorldlessSpaceGameSimulation, EntityRegistry_CountsTeams) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_entity_registry_scenario(config, ml::EEntityRegistryScenario::TeamCounts);
}

TEST(WorldlessSpaceGameSimulation, EntityRegistry_OnePlayerKill) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_entity_registry_scenario(config, ml::EEntityRegistryScenario::OnePlayerKill);
}

TEST(WorldlessSpaceGameSimulation, EntityRegistry_TwoPlayerKills) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_worldless_entity_registry_scenario(config, ml::EEntityRegistryScenario::TwoPlayerKills);
}

TEST(WorldlessSpaceGameSimulation, Orchestrator_FixedStepPauseResumeAndCatchUp) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_worldless_simulation_core_regression(
        config, ml::ESimulationCoreRegressionScenario::FixedTickLifecycle);
}

TEST(WorldlessSpaceGameSimulation, Entities_NonLethalThenLethalDamageCleansUpAtomically) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_worldless_simulation_core_regression(
        config, ml::ESimulationCoreRegressionScenario::DamageLifecycle);
}

TEST(WorldlessSpaceGameSimulation, Entities_CollisionOverlapDamagesAndKills) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_collision_damage(config);
}

TEST(WorldlessSpaceGameSimulation, Mission_SurviveTime) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_mission_manager_scenario(config, ml::EMissionManagerScenario::SurviveTime);
}

TEST(WorldlessSpaceGameSimulation, Mission_KillEnemies) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_mission_manager_scenario(config, ml::EMissionManagerScenario::KillEnemies);
}

TEST(WorldlessSpaceGameSimulation, Mission_KillEnemiesWithinTime) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_mission_manager_scenario(config,
                                               ml::EMissionManagerScenario::KillEnemiesWithinTime);
}

TEST(WorldlessSpaceGameSimulation, Mission_DefenceObjective) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_mission_manager_scenario(config,
                                               ml::EMissionManagerScenario::DefenceObjective);
}

TEST(WorldlessSpaceGameSimulation, Mission_RequiredKillsObjective) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_mission_manager_scenario(config,
                                               ml::EMissionManagerScenario::RequiredKillsObjective);
}

TEST(WorldlessSpaceGameSimulation, Mission_RequiredKillsTimeElapsed) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_mission_manager_scenario(
        config, ml::EMissionManagerScenario::RequiredKillsTimeElapsed);
}

TEST(WorldlessSpaceGameSimulation, Mission_AutomaticKillTargetIncludesLastEnemy) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_mission_manager_scenario(config,
                                               ml::EMissionManagerScenario::AutomaticKillTarget);
}

TEST(WorldlessSpaceGameSimulation, Mission_SuccessIsTerminal) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_mission_manager_scenario(config,
                                               ml::EMissionManagerScenario::SuccessIsTerminal);
}

TEST(WorldlessSpaceGameSimulation, Mission_ExplicitCompletionIsLatched) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_mission_manager_scenario(
        config, ml::EMissionManagerScenario::ExplicitCompletionIsLatched);
}

TEST(WorldlessSpaceGameSimulation, PlayerShip_VersusCapital) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_player_ship_vs_capital(config);
}

TEST(WorldlessSpaceGameSimulation, Lasers_QueuedSpawnHitsOnLaterTick) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_laser_lifecycle(config, ml::ELaserLifecycleScenario::Hit);
}

TEST(WorldlessSpaceGameSimulation, Lasers_SimultaneousHitsCauseOneDeath) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_laser_lifecycle(config, ml::ELaserLifecycleScenario::SimultaneousLethalHits);
}

TEST(WorldlessSpaceGameSimulation, Lasers_MissExpiresWithoutDamage) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_laser_lifecycle(config, ml::ELaserLifecycleScenario::Miss);
}

TEST(WorldlessSpaceGameSimulation, Lasers_WorldBlockerConsumesProjectileWithoutEntityDamage) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_laser_lifecycle(config, ml::ELaserLifecycleScenario::WorldBlocker);
}

TEST(WorldlessSpaceGameSimulation, SpatialQuery_ResolvesLineOfSightBatches) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_spatial_query_line_of_sight(config);
}

TEST(WorldlessSpaceGameSimulation, SpatialQuery_EmptyBatchesAndWorld) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_spatial_query_empty(config);
}

TEST(WorldlessSpaceGameSimulation, SpatialQuery_TeamAndInclusiveRadiusFiltering) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_spatial_query_range(config);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceHitsAndMisses) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(config,
                                         ml::ECollisionUniformGridTraceScenario::HitsAndMisses);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridContainsAllEntityTypes) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_collision_uniform_grid_membership(config);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceStopsAtEndpoint) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(config,
                                         ml::ECollisionUniformGridTraceScenario::StopsAtEndpoint);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceReturnsNearestHit) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(config,
                                         ml::ECollisionUniformGridTraceScenario::ReturnsNearestHit);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceHandlesZeroLengthTraces) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(
        config, ml::ECollisionUniformGridTraceScenario::HandlesZeroLengthTraces);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceIncludesNegativeEndpointBoundary) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(
        config, ml::ECollisionUniformGridTraceScenario::IncludesNegativeEndpointBoundary);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceAppliesAABBCentre) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(config,
                                         ml::ECollisionUniformGridTraceScenario::AppliesAABBCentre);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceAxisParallelAndOrigin) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(
        config, ml::ECollisionUniformGridTraceScenario::AxisParallelAndOrigin);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceSurfaceContacts) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(config,
                                         ml::ECollisionUniformGridTraceScenario::SurfaceContacts);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceGridBoundaryTraversal) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(
        config, ml::ECollisionUniformGridTraceScenario::GridBoundaryTraversal);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceShortAndNearParallelSegments) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(
        config, ml::ECollisionUniformGridTraceScenario::ShortAndNearParallelSegments);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceClipsToGridBounds) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(config,
                                         ml::ECollisionUniformGridTraceScenario::ClipsToGridBounds);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceDegenerateAABBs) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(config,
                                         ml::ECollisionUniformGridTraceScenario::DegenerateAABBs);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceCrossCellNearestHit) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(
        config, ml::ECollisionUniformGridTraceScenario::CrossCellNearestHit);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceVariedGridGeometry) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(
        config, ml::ECollisionUniformGridTraceScenario::VariedGridGeometry);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceBoundaryPrecision) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(config,
                                         ml::ECollisionUniformGridTraceScenario::BoundaryPrecision);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridRebuildLifecycle) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(config,
                                         ml::ECollisionUniformGridTraceScenario::RebuildLifecycle);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceDeterministicReferenceSweep) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(
        config, ml::ECollisionUniformGridTraceScenario::DeterministicReferenceSweep);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceInvarianceProperties) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(
        config, ml::ECollisionUniformGridTraceScenario::InvarianceProperties);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceEmptyBatchesAndOutputReuse) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(
        config, ml::ECollisionUniformGridTraceScenario::EmptyBatchesAndOutputReuse);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridDenseAndWideAABBs) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(config,
                                         ml::ECollisionUniformGridTraceScenario::DenseAndWideAABBs);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceProductionScale) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(config,
                                         ml::ECollisionUniformGridTraceScenario::ProductionScale);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridStaticGeometry) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_collision_uniform_grid_trace(config,
                                         ml::ECollisionUniformGridTraceScenario::StaticGeometry);
}

TEST(WorldlessSpaceGameSimulation, Turrets_LineOfSightBlocking) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_turret_line_of_sight_blocking(config);
}

TEST(WorldlessSpaceGameSimulation, Turrets_KillEnemy) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_turret_combat(config, ml::ETurretCombatScenario::KillEnemy);
}

TEST(WorldlessSpaceGameSimulation, Turrets_ZeroDamage) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_turret_combat(config, ml::ETurretCombatScenario::ZeroDamage);
}

TEST(WorldlessSpaceGameSimulation, Turrets_SearchRequiresLineOfSight) {
    auto const config{ml::simulation_tests::make_fixture()};
    ml::run_worldless_turret_search_requires_line_of_sight(config);
}

TEST(WorldlessSpaceGameSimulation, Turrets_NoOtherEntityRemainsIdle) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_worldless_turret_acquisition_regression(
        config, ml::ETurretAcquisitionRegressionScenario::NoOtherEntity);
}

TEST(WorldlessSpaceGameSimulation, Turrets_FriendlyOnlyRemainsIdle) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_worldless_turret_acquisition_regression(
        config, ml::ETurretAcquisitionRegressionScenario::FriendlyOnly);
}

TEST(WorldlessSpaceGameSimulation, Turrets_EnemyOutsideDetectionRadiusRemainsIdle) {
    auto const config{ml::simulation_tests::make_fixture()};

    ml::run_worldless_turret_acquisition_regression(
        config, ml::ETurretAcquisitionRegressionScenario::EnemyOutsideRadius);
}
