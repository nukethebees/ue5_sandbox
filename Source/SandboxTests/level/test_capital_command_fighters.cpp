#include "test_capital_command_fighters_scenario.h"

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <SandboxGameShared/utilities/enums.h>

#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/time_series_test_data.h>

namespace ml {
FCapitalCommandFightersScenario::FCapitalCommandFightersScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {
    TestCommandBuilder.Do([this] { spawn_fixture(); });
}

/* ------------------------------------------------------------------------------------------ */
// Setup
/* ------------------------------------------------------------------------------------------ */
void FCapitalCommandFightersScenario::spawn_fixture() {
    auto* const green{spawn_capital_proxy(context_.world,
                                          context_.config,
                                          checks,
                                          TEXT("green_capital"),
                                          FVector{-18290.f, 2170.f, 4360.f})};
    auto* const red{spawn_capital_proxy(context_.world,
                                        context_.config,
                                        checks,
                                        TEXT("red_capital"),
                                        FVector{17030.f, 2170.f, 4360.f})};
    auto* const second_red{spawn_capital_proxy(context_.world,
                                               context_.config,
                                               checks,
                                               TEXT("second_red_capital"),
                                               FVector{17030.f, 12170.f, 4360.f})};
    if (!checks.is_valid(green, TEXT("Green capital is spawned")) ||
        !checks.is_valid(red, TEXT("Red capital is spawned")) ||
        !checks.is_valid(second_red, TEXT("Second red capital is spawned"))) {
        return;
    }

    green->set_team(ETestTeam::Green);
    green->set_target_ship(red);
    red->set_team(ETestTeam::Red);
    red->set_target_ship(green);
    second_red->set_team(ETestTeam::Red);
    second_red->set_target_ship(green);
}

void FCapitalCommandFightersScenario::initial_setup() {
    initialise_test_driver();
    test_driver->orchestrator.start_simulation();

    capitals = &test_driver->get_capital_ships();
    fighters = &test_driver->get_capital_ship_fighters();
    team_kept_alive = capitals->get_team(test_capital_idx);
    capital_first_target = capitals->get_target_handle(test_capital_idx);

    auto const capital_fighter_span{capitals->get_capital_fighter_handle_span(test_capital_idx)};
    capital_fighter_start = capital_fighter_span.start();
    capital_fighter_end = capital_fighter_span.end();
}

void FCapitalCommandFightersScenario::initial_setup_and_stimuli() {
    initial_setup();
    reset_and_reserve_time_series(
        test_driver->orchestrator, wait_after_setup + wait_after_kills + wait_after_kills, samples);
    test_driver->orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateRaw(
        this, &FCapitalCommandFightersScenario::on_end_tick));
    test_driver->timeline
        .then_after(wait_after_setup,
                    [this] {
                        t_after_setup = test_driver->get_time();
                        kill_initial_targets();
                    })
        .then_after(wait_after_kills,
                    [this] {
                        t_after_initial_kills = test_driver->get_time();
                        kill_all_not_on_main_team();
                    })
        .then_after(wait_after_kills, [this] { t_after_all_kills = test_driver->get_time(); });
}

/* ------------------------------------------------------------------------------------------ */
// Samples
/* ------------------------------------------------------------------------------------------ */
void FCapitalCommandFightersScenario::sample_values() {
    FSimulationSample sample{};
    sample.capital_target = capitals->get_target_handle(test_capital_idx);
    sample.fighter_targets.Append(fighters->get_target_handles());
    sample.fighter_tasks.Append(fighters->get_tasks());
    sample.capital_count = capitals->get_num_instances();
    samples.add(test_driver->get_time(), MoveTemp(sample));
}

void FCapitalCommandFightersScenario::on_end_tick(ATestBatchOrchestrator&) {
    sample_values();
    test_driver->advance_timeline();
}

/* ------------------------------------------------------------------------------------------ */
// Stimuli and checks
/* ------------------------------------------------------------------------------------------ */
void FCapitalCommandFightersScenario::kill_initial_targets() {
    test_driver->queue_kills(TArray{capitals->get_target_handle(test_capital_idx)});
}

void FCapitalCommandFightersScenario::kill_all_not_on_main_team() {
    auto const handles{test_driver->registry.get_handles_not_in_team(team_kept_alive)};
    test_driver->queue_kills(handles);
}

template <auto EnumValue>
void FCapitalCommandFightersScenario::check_fighter_tasks_are(FSimulationSample const& sample) {
    auto const& tasks{sample.fighter_tasks};
    auto const n{tasks.Num()};
    for (int32 i{0}; i < n; ++i) {
        checks.are_equal(EnumValue,
                         tasks[i],
                         FString::Printf(TEXT("Check fighter %d is in %s"),
                                         i,
                                         *to_string_without_type_prefix(EnumValue)));
    }
}

void FCapitalCommandFightersScenario::check_target_handles(
    FRegistryEntityHandle const capital_target, FSimulationSample const& sample) {
    checks.are_equal(capital_target, sample.capital_target, TEXT("Capital handle hasn't changed"));
    for (int32 i{capital_fighter_start}; i < capital_fighter_end; ++i) {
        checks.are_equal(capital_target,
                         sample.fighter_targets[i],
                         FString::Printf(TEXT("Fighter target handles [%d]"), i));
    }
}

void FCapitalCommandFightersScenario::full_checks() {
    auto const& after_setup{samples.nearest_value(t_after_setup)};
    auto const& after_initial_kills{samples.nearest_value(t_after_initial_kills)};
    auto const& after_all_kills{samples.nearest_value(t_after_all_kills)};

    check_target_handles(capital_first_target, after_setup);
    check_fighter_tasks_are<Task::Attack>(after_setup);
    capital_second_target = after_initial_kills.capital_target;
    checks.is_true(capital_first_target != capital_second_target,
                   TEXT("Capital handles should be different"));
    check_target_handles(capital_second_target, after_initial_kills);
    check_fighter_tasks_are<Task::Standby>(after_all_kills);
    checks.is_greater_than(
        after_all_kills.capital_count, int32{0}, TEXT("At least one capital left"));

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FCapitalCommandFightersScenario::run() {
    TestCommandBuilder.Do([this] { initial_setup_and_stimuli(); })
        .Until([this] { return test_driver->timeline.is_finished(); }, FTimespan{0, 0, 1})
        .Then([this] { full_checks(); });
}
}
