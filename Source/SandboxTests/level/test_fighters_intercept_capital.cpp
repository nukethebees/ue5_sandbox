#include "test_fighters_intercept_capital_scenario.h"

#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFighters.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShips.h>
#include <SpaceGame/entities/TestTeam.h>

#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestFightersInterceptCapitalResults.h>
#include <SandboxTests/support/TestResultAssetIO.h>
#include <SandboxTests/support/time_series_test_data.h>

namespace ml {
FFightersInterceptCapitalScenario::FFightersInterceptCapitalScenario(
    FSimulationTestContext& context)
    : FSimulationTestScenario{context} {
    TestCommandBuilder.Do([this] { spawn_fixture(); });
}

/* ------------------------------------------------------------------------------------------ */
// Setup
/* ------------------------------------------------------------------------------------------ */
void FFightersInterceptCapitalScenario::spawn_fixture() {
    auto* const green{spawn_capital_proxy(context_.world,
                                          context_.config,
                                          checks,
                                          TEXT("green_capital"),
                                          FVector{-61180.f, 2170.f, 4360.f})};
    auto* const red{spawn_capital_proxy(context_.world,
                                        context_.config,
                                        checks,
                                        TEXT("red_capital"),
                                        FVector{77320.f, 2170.f, 4360.f})};
    auto* const blue{spawn_capital_proxy(context_.world,
                                         context_.config,
                                         checks,
                                         TEXT("blue_capital"),
                                         FVector{3590.f, 3240.f, 4360.f})};
    if (!checks.is_valid(green, TEXT("Green capital is spawned")) ||
        !checks.is_valid(red, TEXT("Red capital is spawned")) ||
        !checks.is_valid(blue, TEXT("Blue capital is spawned"))) {
        return;
    }

    green->set_team(ETestTeam::Green);
    green->set_target_ship(red);
    green->set_spawn_cooldown(60.f);
    red->set_team(ETestTeam::Red);
    red->set_target_ship(green);
    red->set_initial_spawn_delay(600.f);
    blue->set_team(ETestTeam::Blue);
    blue->set_target_ship(green);
    blue->set_initial_spawn_delay(600.f);
}

void FFightersInterceptCapitalScenario::initial_setup() {
    initialise_test_driver();
    test_driver->orchestrator.start_simulation();
    capitals = &test_driver->get_capital_ships();
    fighters = &test_driver->get_capital_ship_fighters();

    auto const hero_index{capitals->find_first_index_on_team(ETestTeam::Green)};
    auto const red_handle{capitals->find_first_handle_on_team(ETestTeam::Red)};
    auto const blue_handle{capitals->find_first_handle_on_team(ETestTeam::Blue)};
    if (!checks.is_true(hero_index.has_value(), TEXT("Find green capital")) ||
        !checks.is_true(red_handle.has_value(), TEXT("Find red capital")) ||
        !checks.is_true(blue_handle.has_value(), TEXT("Find blue capital"))) {
        return;
    }
    hero_capital_index = *hero_index;
    hero_capital = capitals->get_handle(hero_capital_index);
    original_target = *red_handle;
    intercept_target = *blue_handle;

    reset_and_reserve_time_series(test_driver->orchestrator, test_duration, samples);
    test_driver->orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateRaw(
        this, &FFightersInterceptCapitalScenario::on_end_tick));
    test_driver->timeline.finish_at(test_duration);
}

/* ------------------------------------------------------------------------------------------ */
// Samples and checks
/* ------------------------------------------------------------------------------------------ */
void FFightersInterceptCapitalScenario::sample_values() {
    auto const fighter_handles{capitals->get_fighter_handles(hero_capital_index)};
    FSimulationSample sample{};
    sample.parent_target = capitals->get_target_handle(hero_capital_index);
    sample.fighter_count = fighter_handles.Num();
    sample.fighter_targets.Reserve(fighter_handles.Num());
    for (auto const fighter_handle : fighter_handles) {
        sample.fighter_targets.Add(fighters->get_target_handle(fighter_handle));
    }
    samples.add(test_driver->get_time(), MoveTemp(sample));
}

void FFightersInterceptCapitalScenario::on_end_tick(ATestBatchOrchestrator&) {
    sample_values();
    test_driver->advance_timeline();
}

void FFightersInterceptCapitalScenario::check_fighters_share_target(FSimulationSample const& sample,
                                                                    FString const& description) {
    if (!checks.is_greater_than(sample.fighter_targets.Num(), int32{0}, description)) {
        return;
    }
    auto const shared_target{sample.fighter_targets[0]};
    for (int32 i{1}; i < sample.fighter_targets.Num(); ++i) {
        checks.are_equal(shared_target, sample.fighter_targets[i], description, i);
    }
}

void FFightersInterceptCapitalScenario::full_checks() {
    auto const& start{samples.nearest_value(initial_setup_duration)};
    auto const& end{samples.nearest_value(test_duration)};
    checks.is_greater_than(start.fighter_targets.Num(), int32{0}, TEXT("Parent has fighters"));
    checks.is_greater_than(end.fighter_targets.Num(), int32{0}, TEXT("Parent has fighters at end"));
    checks.are_equal(
        original_target, start.parent_target, TEXT("Green capital initially targets red capital"));
    for (int32 i{0}; i < start.fighter_targets.Num(); ++i) {
        checks.are_equal(original_target,
                         start.fighter_targets[i],
                         TEXT("Initial fighter target matches red parent target"),
                         i);
    }
    for (int32 i{0}; i < end.fighter_targets.Num(); ++i) {
        checks.are_equal(intercept_target,
                         end.fighter_targets[i],
                         TEXT("Final fighter target is blue capital"),
                         i);
    }
    check_fighters_share_target(start, TEXT("Fighters share their initial target"));
    check_fighters_share_target(end, TEXT("Fighters share their final target"));
}

void FFightersInterceptCapitalScenario::export_data() const {
    auto const result_asset{FTestResultAsset{TEXT("fighters_intercept_capital"), *TestRunner}};
    auto* results{
        result_asset.load_or_create<UTestFightersInterceptCapitalResults>(TEXT("data_asset"))};
    auto* curves{result_asset.load_or_create<UCurveTable>(TEXT("data_curve"))};
    results->hero_capital = hero_capital.to_string();
    results->original_target = original_target.to_string();
    results->intercept_target = intercept_target.to_string();

    auto const sample_times{samples.times()};
    auto const sample_values{samples.values()};
    curves->EmptyTable();
    TArray<float> curve_times;
    TArray<int32> fighter_counts;
    curve_times.Reserve(sample_values.Num());
    fighter_counts.Reserve(sample_values.Num());
    results->time_series_results.Reset(sample_values.Num());
    for (int32 i{0}; i < sample_values.Num(); ++i) {
        auto const& sample{sample_values[i]};
        FFightersInterceptCapitalTimeSeriesRow row{};
        row.time = sample_times[i];
        row.parent_target = sample.parent_target.to_string();
        row.fighter_targets.Reserve(sample.fighter_targets.Num());
        for (auto const target : sample.fighter_targets) {
            row.fighter_targets.Add(target.to_string());
        }
        curve_times.Add(static_cast<float>(sample_times[i]));
        fighter_counts.Add(sample.fighter_count);
        results->time_series_results.Add(MoveTemp(row));
    }
    add_simple_curve_row(*curves,
                         TEXT("fighter_count"),
                         TConstArrayView<int32>{fighter_counts},
                         TConstArrayView<float>{curve_times});
    result_asset.save(*results);
    result_asset.save(*curves);
}

void FFightersInterceptCapitalScenario::run() {
    run_until_timeline_finished([this] { initial_setup(); },
                                FTimespan{0, 0, 21},
                                [this] {
                                    full_checks();
                                    if (!checks.all_passed ||
                                        test_driver->should_export_results()) {
                                        export_data();
                                    }
                                    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
                                });
}
}
