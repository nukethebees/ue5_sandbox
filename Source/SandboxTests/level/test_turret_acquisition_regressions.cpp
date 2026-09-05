#include "test_turret_acquisition_regressions_scenario.h"

#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>

#include <SpaceGame/combat/lasers/TestLasersSimulation.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsConfig.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsSimulation.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxCoreEngine/actor_utils.h>

namespace ml {
FTurretAcquisitionRegressionScenario::FTurretAcquisitionRegressionScenario(
    FSimulationTestContext& context, ETurretAcquisitionRegressionScenario const scenario)
    : FSimulationTestScenario{context}
    , scenario_{scenario} {
    TestCommandBuilder.Do([this] { spawn_fixture(); });
}

void FTurretAcquisitionRegressionScenario::on_tear_down() {
    if (driver.IsSet()) {
        driver->orchestrator.clear_end_tick_test_hook();
    }
}

/* ------------------------------------------------------------------------------------------ */
// Invalid target candidates
/* ------------------------------------------------------------------------------------------ */
void FTurretAcquisitionRegressionScenario::spawn_fixture() {
    auto const initialise{
        [this](ATestStaticTurretsProxy& actor, int32 const i, ESpawnPhase const phase) {
            if (phase == ESpawnPhase::PreSpawn) {
                actor.set_team(scenario_ == ETurretAcquisitionRegressionScenario::FriendlyOnly ||
                                       i == 0
                                   ? ETestTeam::Blue
                                   : ETestTeam::Red);
                actor.set_laser_damage(0);
                return;
            }

            auto location{FVector::ZeroVector};
            if (i == 1) {
                auto const* const turret_config{&context_.config.turrets};
                auto const distance{
                    scenario_ == ETurretAcquisitionRegressionScenario::EnemyOutsideRadius &&
                            turret_config
                        ? turret_config->detection_radius + 1.f
                        : 1000.f};
                location.X = distance;
            }
            actor.SetActorLocation(location);
        }};

    if (scenario_ == ETurretAcquisitionRegressionScenario::NoOtherEntity) {
        spawn_actors<ATestStaticTurretsProxy, 1>(context_.world, initialise);
    } else {
        spawn_actors<ATestStaticTurretsProxy, 2>(context_.world, initialise);
    }
}

void FTurretAcquisitionRegressionScenario::on_end_tick(ATestBatchOrchestrator& orchestrator) {
    auto const* const turrets{orchestrator.get_turrets()};
    check(turrets);
    targets.add(driver->get_time(), TArray<FRegistryEntityHandle>{turrets->get_target_handles()});
    driver->timeline.tick(driver->get_time());
}

void FTurretAcquisitionRegressionScenario::check_results() {
    checks.is_true(!targets.is_empty(), TEXT("Turret targets are sampled"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto const& final_targets{targets.last_value()};
    auto const expected_count{scenario_ == ETurretAcquisitionRegressionScenario::NoOtherEntity ? 1
                                                                                               : 2};
    checks.are_equal(expected_count, final_targets.Num(), TEXT("All turret targets are sampled"));
    for (auto const target : final_targets) {
        checks.is_true(target.is_null(), TEXT("Invalid candidate does not become a target"));
    }
    checks.are_equal(0,
                     driver->orchestrator.get_lasers()->get_number_spawned(),
                     TEXT("Turrets without valid targets do not fire"));
}

void FTurretAcquisitionRegressionScenario::run() {
    TestCommandBuilder
        .Do([this] {
            driver = TestSimulationDriver::from_world(context_.world);
            driver->orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateRaw(
                this, &FTurretAcquisitionRegressionScenario::on_end_tick));
            driver->timeline.finish_at(0.5);
            driver->orchestrator.start_simulation();
        })
        .Until([this] { return driver->timeline.is_finished(); }, FTimespan{0, 0, 2})
        .Then([this] {
            check_results();
            SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
        });
}
}
