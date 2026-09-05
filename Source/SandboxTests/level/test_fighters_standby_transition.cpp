#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestTeam.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersSimulation.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxTests/support/level_checks.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/time_series_test_data.h>
#include "test_fighters_standby_transition_scenario.h"

#include <SandboxCore/time_series_data.h>

#include <Engine/World.h>
#include <Misc/Optional.h>

namespace ml {
FFightersStandbyTransitionScenario::FFightersStandbyTransitionScenario(
    FSimulationTestContext& context)
    : FSimulationTestScenario{context} {
    TestCommandBuilder.Do([this] { spawn_capitals(context_.world, context_.config); });
}

/* ------------------------------------------------------------------------------------------ */
// Setup
/* ------------------------------------------------------------------------------------------ */
void FFightersStandbyTransitionScenario::spawn_capitals(UWorld& world,
                                                        USpaceGameLevelConfig const& config) {
    auto* const hero{ml::spawn_capital_proxy(
        world, config, checks, TEXT("hero_capital"), FVector{-4000.f, 0.f, 0.f})};
    if (!checks.is_valid(hero, TEXT("Hero capital is spawned"))) {
        return;
    }
    hero->set_team(ETestTeam::Green);

    auto* const enemy{ml::spawn_capital_proxy(
        world, config, checks, TEXT("enemy_capital"), FVector{4000.f, 0.f, 0.f})};
    if (!checks.is_valid(enemy, TEXT("Enemy capital is spawned"))) {
        return;
    }
    enemy->set_team(ETestTeam::Red);
}

void FFightersStandbyTransitionScenario::initial_setup() {
    initialise_test_driver();
    test_driver->orchestrator.start_simulation();

    auto const& capitals{test_driver->get_capital_ships()};
    checks.are_equal(2, capitals.get_num_instances(), TEXT("Two capitals are registered"));

    auto const enemy_index{capitals.find_first_index_on_team(ETestTeam::Red)};
    if (checks.is_true(enemy_index.has_value(), TEXT("Find enemy capital"))) {
        enemy_capital = capitals.get_handle(*enemy_index);
    }

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    ml::reset_and_reserve_time_series(
        test_driver->orchestrator, pre_kill_wait + post_kill_wait, samples);
    test_driver->orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateRaw(
        this, &FFightersStandbyTransitionScenario::on_end_tick));
    test_driver->timeline
        .then_after(pre_kill_wait,
                    [this] {
                        pre_kill_time = test_driver->get_time();
                        test_driver->queue_kills(TArray{enemy_capital});
                    })
        .then_after(post_kill_wait, [this] { post_kill_time = test_driver->get_time(); })
        .finish_after(0.0);
}

void FFightersStandbyTransitionScenario::on_end_tick(ATestBatchOrchestrator&) {
    sample_values();
    test_driver->advance_timeline();
}

/* ------------------------------------------------------------------------------------------ */
// Samples
/* ------------------------------------------------------------------------------------------ */
void FFightersStandbyTransitionScenario::sample_values() {
    auto const& capitals{test_driver->get_capital_ships()};
    auto const& fighters{test_driver->get_capital_ship_fighters()};
    auto const fighter_handles{fighters.get_handles()};

    FSimulationSample sample{};
    sample.capital_count = capitals.get_num_instances();
    sample.fighter_handles.Append(fighter_handles);
    sample.fighter_tasks.Append(fighters.get_tasks());
    sample.fighter_velocities.Reserve(fighter_handles.Num());
    for (auto const fighter_handle : fighter_handles) {
        sample.fighter_velocities.Add(test_driver->get_registry().get_velocity(fighter_handle));
    }

    samples.add(test_driver->get_time(), MoveTemp(sample));
}

/* ------------------------------------------------------------------------------------------ */
// Checks
/* ------------------------------------------------------------------------------------------ */
void FFightersStandbyTransitionScenario::check_pre_kill_state(FSimulationSample const& sample) {
    if (!checks.is_greater_than(
            sample.fighter_handles.Num(), int32{0}, TEXT("Fighters spawned before capital kill"))) {
        return;
    }

    if (!checks.are_equal(sample.fighter_handles.Num(),
                          sample.fighter_velocities.Num(),
                          TEXT("Pre-kill fighter handles and velocities have matching counts"))) {
        return;
    }

    bool fighter_was_moving{false};
    for (auto const velocity : sample.fighter_velocities) {
        fighter_was_moving |= !velocity.IsNearlyZero();
    }
    checks.is_true(fighter_was_moving,
                   TEXT("At least one fighter moves before the standby transition"));
}

void FFightersStandbyTransitionScenario::check_post_kill_state(FSimulationSample const& sample) {
    checks.are_equal(1, sample.capital_count, TEXT("One capital remains after kill"));
    if (!checks.is_greater_than(
            sample.fighter_handles.Num(), int32{0}, TEXT("Fighters remain after kill"))) {
        return;
    }

    if (!checks.are_equal(sample.fighter_handles.Num(),
                          sample.fighter_tasks.Num(),
                          TEXT("Fighter handles and tasks have matching counts")) ||
        !checks.are_equal(sample.fighter_handles.Num(),
                          sample.fighter_velocities.Num(),
                          TEXT("Fighter handles and velocities have matching counts"))) {
        return;
    }

    for (int32 i{0}; i < sample.fighter_handles.Num(); ++i) {
        checks.are_equal(
            Task::Standby, sample.fighter_tasks[i], TEXT("Fighter transitioned to standby"), i);
        checks.dist_zero(sample.fighter_velocities[i],
                         FVector3f::ZeroVector,
                         0.0f,
                         TEXT("Registry fighter velocity is zero in standby"),
                         i);
    }
}

void FFightersStandbyTransitionScenario::full_checks() {
    ml::check_samples_recorded(samples.num(), checks, TEXT("Simulation samples recorded"));
    if (!checks.is_true(pre_kill_time.IsSet(), TEXT("Pre-kill sample time was recorded")) ||
        !checks.is_true(post_kill_time.IsSet(), TEXT("Post-kill sample time was recorded"))) {
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
        return;
    }

    check_pre_kill_state(samples.nearest_value(*pre_kill_time));
    check_post_kill_state(samples.nearest_value(*post_kill_time));

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FFightersStandbyTransitionScenario::run() {
    run_until_timeline_finished([this] { initial_setup(); }, timeout, [this] { full_checks(); });
}
}
