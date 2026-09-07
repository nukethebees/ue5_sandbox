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
#include "test_level_loader_scenario.h"
#include "test_mission_manager_scenario.h"
#include "test_player_ship_death_scenario.h"
#include "test_player_ship_vs_capital_scenario.h"
#include "test_simulation_core_regressions_scenario.h"
#include "test_spatial_query_empty_scenario.h"
#include "test_spatial_query_line_of_sight_scenario.h"
#include "test_turret_acquisition_regressions_scenario.h"
#include "test_turret_combat_scenario.h"
#include "test_turret_line_of_sight_blocking_scenario.h"
#include "test_turret_search_requires_line_of_sight_scenario.h"

#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/SpaceGameTestSettings.h>
#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/levels/LevelLoader.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameS7/LevelDefinitionReader.h>

#include <CQTest.h>
#include <HAL/PlatformTime.h>
#include <Misc/Paths.h>

#define SHARED_SIMULATION_TEST(METHOD_NAME, SCENARIO_TYPE, ...) \
    TEST_METHOD(METHOD_NAME)                                    \
    { run_scenario<SCENARIO_TYPE>(__VA_ARGS__); }

TEST_CLASS(SharedSimulation, "Sandbox.LevelTests")
{
    inline static ml::FTestBatchOrchestratorLevelSetup level_setup{};

    ml::FSoftTestAssertions checks{};
    TUniquePtr<ml::FSimulationTestContext> context{nullptr};
    TUniquePtr<ml::FSimulationTestScenario> scenario{nullptr};
    bool level_used{false};

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        auto const& test_name{TestRunner->GetTestFullName()};
        auto const headless_capable{
            test_name.Contains(TEXT(".Mission_")) || test_name.Contains(TEXT(".EntityRegistry_")) ||
            test_name.Contains(TEXT(".Lasers_")) ||
            test_name.EndsWith(TEXT(".PlayerShip_VersusCapital")) ||
            test_name.Contains(TEXT(".SpatialQuery_")) ||
            test_name.Contains(TEXT(".Turrets_NoOtherEntity")) ||
            test_name.Contains(TEXT(".Turrets_FriendlyOnly")) ||
            test_name.Contains(TEXT(".Turrets_EnemyOutside")) ||
            test_name.Contains(TEXT(".Turrets_LineOfSightBlocking")) ||
            test_name.Contains(TEXT(".Turrets_KillEnemy")) ||
            test_name.Contains(TEXT(".Turrets_ZeroDamage")) ||
            test_name.Contains(TEXT(".Turrets_SearchRequiresLineOfSight")) ||
            (test_name.Contains(TEXT(".HUD_")) && !test_name.Contains(TEXT("LateRegistration"))) ||
            test_name.EndsWith(TEXT(".CapitalCommandFighters_RetargetAfterKills")) ||
            test_name.Contains(TEXT(".CapitalFighterHandles_")) ||
            test_name.EndsWith(TEXT(".Orchestrator_FixedStepPauseResumeAndCatchUp")) ||
            test_name.EndsWith(TEXT(".Entities_NonLethalThenLethalDamageCleansUpAtomically")) ||
            test_name.EndsWith(TEXT(".Fighters_LineOfSightFailureHandling")) ||
            test_name.EndsWith(TEXT(".Fighters_InterceptCapital")) ||
            test_name.EndsWith(TEXT(".Fighters_StandbyTransition")) ||
            test_name.EndsWith(TEXT(".Fighters_AttackCapital")) ||
            test_name.EndsWith(TEXT(".Fighters_ObstacleAvoidance")) ||
            test_name.Contains(TEXT(".Fighters_Navigation"))};
        level_used = !headless_capable;
        if (level_used) {
            level_setup.begin_test(TestCommandBuilder, *TestRunner, checks);
        }
    }

    AFTER_EACH()
    {
        if (scenario) {
            scenario->tear_down();
        }
        scenario.Reset();
        context.Reset();
        if (level_used) {
            level_setup.end_test();
        }
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

    void run_mission_scenario(ml::EMissionManagerScenario const mission_scenario) {
        TestCommandBuilder.Do([this, mission_scenario] {
            auto const* config{ml::get_default_level_config(checks)};
            if (config) {
                ml::run_worldless_mission_manager_scenario(*TestRunner, *config, mission_scenario);
            }
        });
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
    SHARED_SIMULATION_TEST(LevelLoader_MaterialisesDefinition, ml::FLevelLoaderScenario)
    SHARED_SIMULATION_TEST(LevelLoader_MaterialisesPlayerlessCameraDefinition,
                           ml::FLevelLoaderCameraScenario)

    SHARED_SIMULATION_TEST(CapitalShipProxy_HealthOverridesConfig,
                           ml::FTestCapitalShipProxyScenario)
    TEST_METHOD(CapitalCommandFighters_RetargetAfterKills)
    {
        run_worldless_capable_scenario<ml::FCapitalCommandFightersScenario>(
            ml::run_worldless_capital_command_fighters);
    }
    TEST_METHOD(CapitalFighterHandles_KillFightersOnly)
    { run_capital_fighter_handles(ml::ECapitalFighterHandlesScenario::KillFightersOnly); }
    TEST_METHOD(CapitalFighterHandles_KillCapital)
    { run_capital_fighter_handles(ml::ECapitalFighterHandlesScenario::KillCapital); }
    TEST_METHOD(CapitalFighterHandles_All)
    { run_capital_fighter_handles(ml::ECapitalFighterHandlesScenario::All); }

    TEST_METHOD(Fighters_LineOfSightFailureHandling)
    {
        TestCommandBuilder.Do([this] {
            auto const* config{ml::get_default_level_config(checks)};
            if (config) {
                ml::run_worldless_fighter_los_failure(*TestRunner, checks, *config);
            }
        });
    }

    void run_entity_registry_scenario(ml::EEntityRegistryScenario const registry_scenario) {
        TestCommandBuilder.Do([this, registry_scenario] {
            auto const* config{ml::get_default_level_config(checks)};
            if (config) {
                ml::run_worldless_entity_registry_scenario(
                    *TestRunner, checks, *config, registry_scenario);
            }
        });
    }

    template <typename Scenario, typename WorldlessRunner>
    void run_worldless_capable_scenario(WorldlessRunner && worldless_runner) {
        TestCommandBuilder.Do([this, runner = Forward<WorldlessRunner>(worldless_runner)] {
            auto const* config{ml::get_default_level_config(checks)};
            if (config) {
                runner(*TestRunner, checks, *config);
            }
        });
    }

    void run_laser_lifecycle(ml::ELaserLifecycleScenario const laser_scenario) {
        TestCommandBuilder.Do([this, laser_scenario] {
            auto const* config{ml::get_default_level_config(checks)};
            if (config) {
                ml::run_worldless_laser_lifecycle(*TestRunner, checks, *config, laser_scenario);
            }
        });
    }

    void run_turret_acquisition(
        ml::ETurretAcquisitionRegressionScenario const acquisition_scenario) {
        TestCommandBuilder.Do([this, acquisition_scenario] {
            auto const* config{ml::get_default_level_config(checks)};
            if (config) {
                ml::run_worldless_turret_acquisition_regression(
                    *TestRunner, checks, *config, acquisition_scenario);
            }
        });
    }

    void run_turret_combat(ml::ETurretCombatScenario const combat_scenario) {
        TestCommandBuilder.Do([this, combat_scenario] {
            auto const* config{ml::get_default_level_config(checks)};
            if (config) {
                ml::run_worldless_turret_combat(*TestRunner, checks, *config, combat_scenario);
            }
        });
    }

    void run_capital_fighter_handles(
        ml::ECapitalFighterHandlesScenario const fighter_handles_scenario) {
        TestCommandBuilder.Do([this, fighter_handles_scenario] {
            auto const* config{ml::get_default_level_config(checks)};
            if (config) {
                ml::run_worldless_capital_fighter_handles(
                    *TestRunner, checks, *config, fighter_handles_scenario);
            }
        });
    }

    void run_hud_manager(ml::EHUDManagerScenario const hud_scenario) {
        TestCommandBuilder.Do([this, hud_scenario] {
            auto const* config{ml::get_default_level_config(checks)};
            if (config) {
                ml::run_worldless_hud_manager_scenario(*TestRunner, checks, *config, hud_scenario);
            }
        });
    }

    void run_core_regression(ml::ESimulationCoreRegressionScenario const regression_scenario) {
        TestCommandBuilder.Do([this, regression_scenario] {
            auto const* config{ml::get_default_level_config(checks)};
            if (config) {
                ml::run_worldless_simulation_core_regression(
                    *TestRunner, checks, *config, regression_scenario);
            }
        });
    }
    SHARED_SIMULATION_TEST(EntityInterface_ConvertsProxiesAndResolvesTargets,
                           ml::FEntityInterfaceScenario)
    TEST_METHOD(EntityRegistry_CountsTeams)
    { run_entity_registry_scenario(ml::EEntityRegistryScenario::TeamCounts); }
    TEST_METHOD(EntityRegistry_OnePlayerKill)
    { run_entity_registry_scenario(ml::EEntityRegistryScenario::OnePlayerKill); }
    TEST_METHOD(EntityRegistry_TwoPlayerKills)
    { run_entity_registry_scenario(ml::EEntityRegistryScenario::TwoPlayerKills); }
    TEST_METHOD(Fighters_StandbyTransition)
    {
        run_worldless_capable_scenario<ml::FFightersStandbyTransitionScenario>(
            ml::run_worldless_fighters_standby_transition);
    }
    TEST_METHOD(Fighters_InterceptCapital)
    {
        TestCommandBuilder.Do([this] {
            auto const* config{ml::get_default_level_config(checks)};
            if (config) {
                ml::run_worldless_fighters_intercept_capital(*TestRunner, checks, *config);
            }
        });
    }
    TEST_METHOD(Fighters_AttackCapital)
    {
        run_worldless_capable_scenario<ml::FFighterAttackScenario>(
            ml::run_worldless_fighter_attack);
    }
    TEST_METHOD(Fighters_ObstacleAvoidance)
    {
        TestCommandBuilder.Do([this] {
            auto const* config{ml::get_default_level_config(checks)};
            if (config) {
                ml::run_worldless_fighter_obstacle_avoidance(*TestRunner, checks, *config);
            }
        });
    }
    TEST_METHOD(Fighters_NavigationCapitalObstruction)
    {
        run_worldless_capable_scenario<ml::FFighterAttackScenario>(
            ml::run_worldless_fighter_capital_obstruction);
    }
    TEST_METHOD(Fighters_NavigationClearPath)
    {
        run_worldless_capable_scenario<ml::FFighterAttackScenario>(
            ml::run_worldless_fighter_clear_navigation);
    }
    TEST_METHOD(Fighters_NavigationSeparation)
    {
        run_worldless_capable_scenario<ml::FFighterAttackScenario>(
            ml::run_worldless_fighter_separation);
    }
    TEST_METHOD(Fighters_NavigationDenseDeterminism)
    {
        run_worldless_capable_scenario<ml::FFighterAttackScenario>(
            ml::run_worldless_fighter_dense_determinism);
    }
    TEST_METHOD(Fighters_NavigationLargeCluster)
    {
        run_worldless_capable_scenario<ml::FFighterAttackScenario>(
            ml::run_worldless_fighter_large_cluster);
    }
    TEST_METHOD(Fighters_NavigationHardAvoidanceAuthority)
    {
        run_worldless_capable_scenario<ml::FFighterAttackScenario>(
            ml::run_worldless_fighter_hard_avoidance_authority);
    }
    TEST_METHOD(Fighters_NavigationFrequency)
    {
        run_worldless_capable_scenario<ml::FFighterAttackScenario>(
            ml::run_worldless_fighter_navigation_frequency);
    }

    TEST_METHOD(HUD_InitialCachesPopulateWithoutHUD)
    { run_hud_manager(ml::EHUDManagerScenario::InitialCachesPopulateWithoutHUD); }
    TEST_METHOD(HUD_EntityCountPollingContinuesWithoutHUD)
    { run_hud_manager(ml::EHUDManagerScenario::EntityCountPollingContinuesWithoutHUD); }
    TEST_METHOD(HUD_MissionAndDefenceDataUpdateWithoutHUD)
    { run_hud_manager(ml::EHUDManagerScenario::MissionAndDefenceDataUpdateWithoutHUD); }
    TEST_METHOD(HUD_PlayerStateAndKillsUpdateWithoutHUD)
    { run_hud_manager(ml::EHUDManagerScenario::PlayerStateAndKillsUpdateWithoutHUD); }
    TEST_METHOD(HUD_MissionTimeUsesSimulationClockWithoutHUD)
    { run_hud_manager(ml::EHUDManagerScenario::MissionTimeUsesSimulationClockWithoutHUD); }
    SHARED_SIMULATION_TEST(HUD_LateRegistrationSynchronisesAndUnregisters,
                           ml::FTestHUDManagerScenario,
                           ml::EHUDManagerScenario::LateHUDRegistrationSynchronisesAndUnregisters)

    TEST_METHOD(Orchestrator_FixedStepPauseResumeAndCatchUp)
    { run_core_regression(ml::ESimulationCoreRegressionScenario::FixedTickLifecycle); }
    TEST_METHOD(Entities_NonLethalThenLethalDamageCleansUpAtomically)
    { run_core_regression(ml::ESimulationCoreRegressionScenario::DamageLifecycle); }

    TEST_METHOD(Mission_SurviveTime)
    { run_mission_scenario(ml::EMissionManagerScenario::SurviveTime); }
    TEST_METHOD(Mission_KillEnemies)
    { run_mission_scenario(ml::EMissionManagerScenario::KillEnemies); }
    TEST_METHOD(Mission_KillEnemiesWithinTime)
    { run_mission_scenario(ml::EMissionManagerScenario::KillEnemiesWithinTime); }
    TEST_METHOD(Mission_DefenceObjective)
    { run_mission_scenario(ml::EMissionManagerScenario::DefenceObjective); }
    TEST_METHOD(Mission_RequiredKillsObjective)
    { run_mission_scenario(ml::EMissionManagerScenario::RequiredKillsObjective); }
    TEST_METHOD(Mission_RequiredKillsTimeElapsed)
    { run_mission_scenario(ml::EMissionManagerScenario::RequiredKillsTimeElapsed); }
    TEST_METHOD(Mission_AutomaticKillTargetIncludesLastEnemy)
    { run_mission_scenario(ml::EMissionManagerScenario::AutomaticKillTarget); }
    TEST_METHOD(Mission_SuccessIsTerminal)
    { run_mission_scenario(ml::EMissionManagerScenario::SuccessIsTerminal); }
    TEST_METHOD(Mission_ExplicitCompletionIsLatched)
    { run_mission_scenario(ml::EMissionManagerScenario::ExplicitCompletionIsLatched); }

    SHARED_SIMULATION_TEST(PlayerShip_LethalDamageDestroysPlayerShip,
                           ml::FTestPlayerShipDeathScenario)
    TEST_METHOD(PlayerShip_VersusCapital)
    {
        run_worldless_capable_scenario<ml::FPlayerShipVsCapitalScenario>(
            ml::run_worldless_player_ship_vs_capital);
    }

    TEST_METHOD(Lasers_QueuedSpawnHitsOnLaterTick)
    { run_laser_lifecycle(ml::ELaserLifecycleScenario::Hit); }
    TEST_METHOD(Lasers_SimultaneousHitsCauseOneDeath)
    { run_laser_lifecycle(ml::ELaserLifecycleScenario::SimultaneousLethalHits); }
    TEST_METHOD(Lasers_MissExpiresWithoutDamage)
    { run_laser_lifecycle(ml::ELaserLifecycleScenario::Miss); }
    TEST_METHOD(Lasers_WorldBlockerConsumesProjectileWithoutEntityDamage)
    { run_laser_lifecycle(ml::ELaserLifecycleScenario::WorldBlocker); }

    TEST_METHOD(SpatialQuery_ResolvesLineOfSightBatches)
    {
        run_worldless_capable_scenario<ml::FSpatialQueryLineOfSightScenario>(
            ml::run_worldless_spatial_query_line_of_sight);
    }
    TEST_METHOD(SpatialQuery_EmptyBatchesAndWorld)
    {
        run_worldless_capable_scenario<ml::FSpatialQueryEmptyScenario>(
            ml::run_worldless_spatial_query_empty);
    }
    TEST_METHOD(SpatialQuery_TeamAndInclusiveRadiusFiltering)
    {
        run_worldless_capable_scenario<ml::FSpatialQueryRangeScenario>(
            ml::run_worldless_spatial_query_range);
    }
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
    SHARED_SIMULATION_TEST(Collision_UniformGridStaticGeometry,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::StaticGeometry)
    SHARED_SIMULATION_TEST(Collision_UniformGridStaticHarvesting,
                           ml::FCollisionUniformGridTraceScenario,
                           ml::ECollisionUniformGridTraceScenario::StaticHarvesting)

    TEST_METHOD(Turrets_LineOfSightBlocking)
    {
        run_worldless_capable_scenario<ml::FTurretLineOfSightBlockingScenario>(
            ml::run_worldless_turret_line_of_sight_blocking);
    }
    TEST_METHOD(Turrets_KillEnemy)
    { run_turret_combat(ml::ETurretCombatScenario::KillEnemy); }
    TEST_METHOD(Turrets_ZeroDamage)
    { run_turret_combat(ml::ETurretCombatScenario::ZeroDamage); }
    TEST_METHOD(Turrets_SearchRequiresLineOfSight)
    {
        run_worldless_capable_scenario<ml::FTurretSearchRequiresLineOfSightScenario>(
            ml::run_worldless_turret_search_requires_line_of_sight);
    }
    TEST_METHOD(Turrets_NoOtherEntityRemainsIdle)
    { run_turret_acquisition(ml::ETurretAcquisitionRegressionScenario::NoOtherEntity); }
    TEST_METHOD(Turrets_FriendlyOnlyRemainsIdle)
    { run_turret_acquisition(ml::ETurretAcquisitionRegressionScenario::FriendlyOnly); }
    TEST_METHOD(Turrets_EnemyOutsideDetectionRadiusRemainsIdle)
    { run_turret_acquisition(ml::ETurretAcquisitionRegressionScenario::EnemyOutsideRadius); }
};

#undef SHARED_SIMULATION_TEST

TEST_CLASS(TelemetryBenchmark, "Sandbox.TelemetryBenchmark")
{
    inline static ml::FTestBatchOrchestratorLevelSetup level_setup{};

    ml::FSoftTestAssertions checks{};

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        level_setup.begin_test(TestCommandBuilder, *TestRunner, checks);
    }

    AFTER_EACH()
    { level_setup.end_test(); }

    AFTER_ALL()
    { level_setup.teardown(); }

    TEST_METHOD(DetailedTimingThroughput)
    {
        TestCommandBuilder.Do([this] {
            auto* const orchestrator{level_setup.get_orchestrator()};
            if (!checks.is_valid(orchestrator, TEXT("Telemetry benchmark orchestrator is valid"))) {
                return;
            }

            ml::s7::FLevelDefinitionReader reader;
            auto const script_path{FPaths::Combine(
                FPaths::ProjectDir(), TEXT("LevelScripts"), TEXT("SparkRendererShowcase.scm"))};
            auto const scripted_definition{reader.read_file(script_path)};
            if (!checks.is_true(static_cast<bool>(scripted_definition),
                                TEXT("Telemetry benchmark battle loads"))) {
                return;
            }

            constexpr auto simulated_seconds{300.0};
            constexpr auto frame_seconds{1.0 / 60.0};
            constexpr auto time_scale{100.0};
            constexpr auto measured_pairs{7};
            TArray<double> timing_off_throughput;
            TArray<double> timing_on_throughput;
            timing_off_throughput.Reserve(measured_pairs);
            timing_on_throughput.Reserve(measured_pairs);

            auto run_trial = [&](bool const detailed_timing) {
                orchestrator->reset_for_new_level();
                orchestrator->set_presentation_enabled(false);
                ml::FLevelLoader loader{*orchestrator};
                auto const load_result{loader.load(scripted_definition.definition.GetValue())};
                if (!checks.is_true(static_cast<bool>(load_result),
                                    TEXT("Telemetry benchmark trial loads"))) {
                    return 0.0;
                }
                orchestrator->start_simulation();
                orchestrator->set_time_scale(time_scale);

                auto& telemetry{orchestrator->get_level_telemetry_manager()};
                FLevelTelemetryRunMetadata metadata;
                metadata.level_id = TEXT("spark-renderer-showcase");
                metadata.presentation_mode = TEXT("simulation_only");
                metadata.requested_duration_seconds = simulated_seconds;
                metadata.initial_requested_time_scale = time_scale;
                metadata.detailed_timing = detailed_timing;
                telemetry.begin_run(MoveTemp(metadata));

                auto const started_at{FPlatformTime::Seconds()};
                while (orchestrator->get_simulation_time() < simulated_seconds) {
                    orchestrator->tick(frame_seconds);
                }
                auto const elapsed_seconds{FPlatformTime::Seconds() - started_at};
                return static_cast<double>(orchestrator->get_completed_ticks()) / elapsed_seconds;
            };

            run_trial(false);
            run_trial(true);
            for (int32 pair_index{0}; pair_index < measured_pairs; ++pair_index) {
                if ((pair_index & 1) == 0) {
                    timing_off_throughput.Add(run_trial(false));
                    timing_on_throughput.Add(run_trial(true));
                } else {
                    timing_on_throughput.Add(run_trial(true));
                    timing_off_throughput.Add(run_trial(false));
                }
            }

            timing_off_throughput.Sort();
            timing_on_throughput.Sort();
            auto const median_index{measured_pairs / 2};
            auto const timing_off_median{timing_off_throughput[median_index]};
            auto const timing_on_median{timing_on_throughput[median_index]};
            auto const throughput_impact_percent{(timing_off_median - timing_on_median) /
                                                 timing_off_median * 100.0};
            TestRunner->AddInfo(FString::Printf(
                TEXT("Telemetry detailed timing A/B: battle=spark-renderer-showcase, "
                     "presentation=simulation_only, duration=%.0fs, time_scale=%.0fx, pairs=%d, "
                     "off_median=%.3f ticks/s, on_median=%.3f ticks/s, impact=%.3f%%"),
                simulated_seconds,
                time_scale,
                measured_pairs,
                timing_off_median,
                timing_on_median,
                throughput_impact_percent));
            TestRunner->TestTrue(TEXT("Detailed timing median throughput impact is below 2%"),
                                 throughput_impact_percent < 2.0);
        });
    }
};
