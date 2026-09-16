#include "scenarios/test_capital_command_fighters.h"
#include "scenarios/test_collision_uniform_grid.h"
#include "scenarios/test_entity_ledger.h"
#include "scenarios/test_fighter_attack.h"
#include "scenarios/test_fighter_los_failure.h"
#include "scenarios/test_fighter_ownership.h"
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

namespace ioj::sim::tests {

TEST(WorldlessSpaceGameSimulation, CapitalCommandFighters_RetargetAfterKills) {
    auto const config{tests::make_fixture()};
    run_worldless_capital_command_fighters(config);
}

TEST(WorldlessSpaceGameSimulation, FighterOwnership_KillFightersOnly) {
    auto const config{tests::make_fixture()};

    run_worldless_fighter_ownership(config, FighterOwnershipScenario::KillFightersOnly);
}

TEST(WorldlessSpaceGameSimulation, FighterOwnership_KillCapital) {
    auto const config{tests::make_fixture()};

    run_worldless_fighter_ownership(config, FighterOwnershipScenario::KillCapital);
}

TEST(WorldlessSpaceGameSimulation, FighterOwnership_All) {
    auto const config{tests::make_fixture()};
    run_worldless_fighter_ownership(config, FighterOwnershipScenario::All);
}

TEST(WorldlessSpaceGameSimulation, Capitals_SimultaneousReassignment) {
    auto const config{tests::make_fixture()};
    run_worldless_simultaneous_capital_reassignment(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_LineOfSightFailureHandling) {
    auto const config{tests::make_fixture()};
    run_worldless_fighter_los_failure(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_StandbyTransition) {
    auto const config{tests::make_fixture()};
    run_worldless_fighters_standby_transition(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_InterceptCapital) {
    auto const config{tests::make_fixture()};
    run_worldless_fighters_intercept_capital(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_AttackCapital) {
    auto const config{tests::make_fixture()};
    run_worldless_fighter_attack(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_ObstacleAvoidance) {
    auto const config{tests::make_fixture()};
    run_worldless_fighter_obstacle_avoidance(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_NavigationCapitalObstruction) {
    auto const config{tests::make_fixture()};
    run_worldless_fighter_capital_obstruction(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_NavigationClearPath) {
    auto const config{tests::make_fixture()};
    run_worldless_fighter_clear_navigation(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_NavigationSeparation) {
    auto const config{tests::make_fixture()};
    run_worldless_fighter_separation(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_NavigationDenseDeterminism) {
    auto const config{tests::make_fixture()};
    run_worldless_fighter_dense_determinism(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_NavigationLargeCluster) {
    auto const config{tests::make_fixture()};
    run_worldless_fighter_large_cluster(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_NavigationHardAvoidanceAuthority) {
    auto const config{tests::make_fixture()};
    run_worldless_fighter_hard_avoidance_authority(config);
}

TEST(WorldlessSpaceGameSimulation, Fighters_NavigationFrequency) {
    auto const config{tests::make_fixture()};
    run_worldless_fighter_navigation_frequency(config);
}

TEST(WorldlessSpaceGameSimulation, EntityLedger_CountsTeams) {
    auto const config{tests::make_fixture()};
    run_worldless_entity_ledger_scenario(config, EntityLedgerScenario::TeamCounts);
}

TEST(WorldlessSpaceGameSimulation, EntityLedger_OnePlayerKill) {
    auto const config{tests::make_fixture()};
    run_worldless_entity_ledger_scenario(config, EntityLedgerScenario::OnePlayerKill);
}

TEST(WorldlessSpaceGameSimulation, EntityLedger_TwoPlayerKills) {
    auto const config{tests::make_fixture()};

    run_worldless_entity_ledger_scenario(config, EntityLedgerScenario::TwoPlayerKills);
}

TEST(WorldlessSpaceGameSimulation, Orchestrator_FixedStepPauseResumeAndCatchUp) {
    auto const config{tests::make_fixture()};

    run_worldless_simulation_core_regression(config,
                                             SimulationCoreRegressionScenario::FixedTickLifecycle);
}

TEST(WorldlessSpaceGameSimulation, Entities_NonLethalThenLethalDamageCleansUpAtomically) {
    auto const config{tests::make_fixture()};

    run_worldless_simulation_core_regression(config,
                                             SimulationCoreRegressionScenario::DamageLifecycle);
}

TEST(WorldlessSpaceGameSimulation, Entities_CollisionOverlapDamagesAndKills) {
    auto const config{tests::make_fixture()};
    run_worldless_collision_damage(config);
}

TEST(WorldlessSpaceGameSimulation, Mission_SurviveTime) {
    auto const config{tests::make_fixture()};
    run_worldless_mission_manager_scenario(config, MissionManagerScenario::SurviveTime);
}

TEST(WorldlessSpaceGameSimulation, Mission_KillEnemies) {
    auto const config{tests::make_fixture()};
    run_worldless_mission_manager_scenario(config, MissionManagerScenario::KillEnemies);
}

TEST(WorldlessSpaceGameSimulation, Mission_KillEnemiesWithinTime) {
    auto const config{tests::make_fixture()};
    run_worldless_mission_manager_scenario(config, MissionManagerScenario::KillEnemiesWithinTime);
}

TEST(WorldlessSpaceGameSimulation, Mission_DefenceObjective) {
    auto const config{tests::make_fixture()};
    run_worldless_mission_manager_scenario(config, MissionManagerScenario::DefenceObjective);
}

TEST(WorldlessSpaceGameSimulation, Mission_RequiredKillsObjective) {
    auto const config{tests::make_fixture()};
    run_worldless_mission_manager_scenario(config, MissionManagerScenario::RequiredKillsObjective);
}

TEST(WorldlessSpaceGameSimulation, Mission_RequiredKillsTimeElapsed) {
    auto const config{tests::make_fixture()};
    run_worldless_mission_manager_scenario(config,
                                           MissionManagerScenario::RequiredKillsTimeElapsed);
}

TEST(WorldlessSpaceGameSimulation, Mission_AutomaticKillTargetIncludesLastEnemy) {
    auto const config{tests::make_fixture()};
    run_worldless_mission_manager_scenario(config, MissionManagerScenario::AutomaticKillTarget);
}

TEST(WorldlessSpaceGameSimulation, Mission_SuccessIsTerminal) {
    auto const config{tests::make_fixture()};
    run_worldless_mission_manager_scenario(config, MissionManagerScenario::SuccessIsTerminal);
}

TEST(WorldlessSpaceGameSimulation, Mission_ExplicitCompletionIsLatched) {
    auto const config{tests::make_fixture()};
    run_worldless_mission_manager_scenario(config,
                                           MissionManagerScenario::ExplicitCompletionIsLatched);
}

TEST(WorldlessSpaceGameSimulation, PlayerShip_VersusCapital) {
    auto const config{tests::make_fixture()};
    run_worldless_player_ship_vs_capital(config);
}

TEST(WorldlessSpaceGameSimulation, Lasers_QueuedSpawnHitsOnLaterTick) {
    auto const config{tests::make_fixture()};
    run_worldless_laser_lifecycle(config, LaserLifecycleScenario::Hit);
}

TEST(WorldlessSpaceGameSimulation, Lasers_SimultaneousHitsCauseOneDeath) {
    auto const config{tests::make_fixture()};
    run_worldless_laser_lifecycle(config, LaserLifecycleScenario::SimultaneousLethalHits);
}

TEST(WorldlessSpaceGameSimulation, Lasers_MissExpiresWithoutDamage) {
    auto const config{tests::make_fixture()};
    run_worldless_laser_lifecycle(config, LaserLifecycleScenario::Miss);
}

TEST(WorldlessSpaceGameSimulation, Lasers_WorldBlockerConsumesProjectileWithoutEntityDamage) {
    auto const config{tests::make_fixture()};
    run_worldless_laser_lifecycle(config, LaserLifecycleScenario::WorldBlocker);
}

TEST(WorldlessSpaceGameSimulation, SpatialQuery_ResolvesLineOfSightBatches) {
    auto const config{tests::make_fixture()};
    run_worldless_spatial_query_line_of_sight(config);
}

TEST(WorldlessSpaceGameSimulation, SpatialQuery_EmptyBatchesAndWorld) {
    auto const config{tests::make_fixture()};
    run_worldless_spatial_query_empty(config);
}

TEST(WorldlessSpaceGameSimulation, SpatialQuery_TeamAndInclusiveRadiusFiltering) {
    auto const config{tests::make_fixture()};
    run_worldless_spatial_query_range(config);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceHitsAndMisses) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(config, CollisionUniformGridTraceScenario::HitsAndMisses);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridContainsAllEntityTypes) {
    auto const config{tests::make_fixture()};
    run_worldless_collision_uniform_grid_membership(config);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceStopsAtEndpoint) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(config, CollisionUniformGridTraceScenario::StopsAtEndpoint);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceReturnsNearestHit) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(config, CollisionUniformGridTraceScenario::ReturnsNearestHit);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceHandlesZeroLengthTraces) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(config,
                                     CollisionUniformGridTraceScenario::HandlesZeroLengthTraces);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceIncludesNegativeEndpointBoundary) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(
        config, CollisionUniformGridTraceScenario::IncludesNegativeEndpointBoundary);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceAppliesAABBCentre) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(config, CollisionUniformGridTraceScenario::AppliesAABBCentre);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceAxisParallelAndOrigin) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(config,
                                     CollisionUniformGridTraceScenario::AxisParallelAndOrigin);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceSurfaceContacts) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(config, CollisionUniformGridTraceScenario::SurfaceContacts);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceGridBoundaryTraversal) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(config,
                                     CollisionUniformGridTraceScenario::GridBoundaryTraversal);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceShortAndNearParallelSegments) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(
        config, CollisionUniformGridTraceScenario::ShortAndNearParallelSegments);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceClipsToGridBounds) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(config, CollisionUniformGridTraceScenario::ClipsToGridBounds);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceDegenerateAABBs) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(config, CollisionUniformGridTraceScenario::DegenerateAABBs);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceCrossCellNearestHit) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(config,
                                     CollisionUniformGridTraceScenario::CrossCellNearestHit);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceVariedGridGeometry) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(config, CollisionUniformGridTraceScenario::VariedGridGeometry);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceBoundaryPrecision) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(config, CollisionUniformGridTraceScenario::BoundaryPrecision);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridRebuildLifecycle) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(config, CollisionUniformGridTraceScenario::RebuildLifecycle);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceDeterministicReferenceSweep) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(
        config, CollisionUniformGridTraceScenario::DeterministicReferenceSweep);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceInvarianceProperties) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(config,
                                     CollisionUniformGridTraceScenario::InvarianceProperties);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceEmptyBatchesAndOutputReuse) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(config,
                                     CollisionUniformGridTraceScenario::EmptyBatchesAndOutputReuse);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridDenseAndWideAABBs) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(config, CollisionUniformGridTraceScenario::DenseAndWideAABBs);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridTraceProductionScale) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(config, CollisionUniformGridTraceScenario::ProductionScale);
}

TEST(WorldlessSpaceGameSimulation, Collision_UniformGridStaticGeometry) {
    auto const config{tests::make_fixture()};

    run_collision_uniform_grid_trace(config, CollisionUniformGridTraceScenario::StaticGeometry);
}

TEST(WorldlessSpaceGameSimulation, Turrets_LineOfSightBlocking) {
    auto const config{tests::make_fixture()};
    run_worldless_turret_line_of_sight_blocking(config);
}

TEST(WorldlessSpaceGameSimulation, Turrets_KillEnemy) {
    auto const config{tests::make_fixture()};
    run_worldless_turret_combat(config, TurretCombatScenario::KillEnemy);
}

TEST(WorldlessSpaceGameSimulation, Turrets_ZeroDamage) {
    auto const config{tests::make_fixture()};
    run_worldless_turret_combat(config, TurretCombatScenario::ZeroDamage);
}

TEST(WorldlessSpaceGameSimulation, Turrets_SearchRequiresLineOfSight) {
    auto const config{tests::make_fixture()};
    run_worldless_turret_search_requires_line_of_sight(config);
}

TEST(WorldlessSpaceGameSimulation, Turrets_NoOtherEntityRemainsIdle) {
    auto const config{tests::make_fixture()};

    run_worldless_turret_acquisition_regression(config,
                                                TurretAcquisitionRegressionScenario::NoOtherEntity);
}

TEST(WorldlessSpaceGameSimulation, Turrets_FriendlyOnlyRemainsIdle) {
    auto const config{tests::make_fixture()};

    run_worldless_turret_acquisition_regression(config,
                                                TurretAcquisitionRegressionScenario::FriendlyOnly);
}

TEST(WorldlessSpaceGameSimulation, Turrets_EnemyOutsideDetectionRadiusRemainsIdle) {
    auto const config{tests::make_fixture()};

    run_worldless_turret_acquisition_regression(
        config, TurretAcquisitionRegressionScenario::EnemyOutsideRadius);
}

} // namespace tests
