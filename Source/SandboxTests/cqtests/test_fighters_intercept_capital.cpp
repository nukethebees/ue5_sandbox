#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestTeam.h>

#include <SandboxTests/cqtests/SoftTestAssertions.h>
#include <SandboxTests/cqtests/test_setup.h>
#include <SandboxTests/cqtests/TestFightersInterceptCapitalResults.h>
#include <SandboxTests/cqtests/TestResultAssetIO.h>
#include <SandboxTests/cqtests/TestSimulationDriver.h>

#include <SandboxCore/time_series_data.h>

#include <Components/MapTestSpawner.h>
#include <CQTest.h>
#include <Misc/Optional.h>

TEST_CLASS(FightersInterceptCapital, "Sandbox.FunctionalTests")
{
    using ThisClass = FightersInterceptCapital;
    using time_type = ml::TestSimulationDriver::time_type;

    struct FSimulationSample {
        FRegistryEntityHandle parent_target;
        TArray<FRegistryEntityHandle> fighter_targets;
        int32 fighter_count{0};
    };

    static constexpr time_type test_duration{20.0};
    static constexpr time_type initial_setup_duration{2.0 / 60.0};

    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    ml::FSoftTestAssertions checks{};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};

    ATestCapitalShips const* capitals{nullptr};
    ATestCapitalShipFighters const* fighters{nullptr};
    ml::TimeSeriesData<FSimulationSample> samples;

    int32 hero_capital_index{INDEX_NONE};
    FRegistryEntityHandle hero_capital;
    FRegistryEntityHandle original_target;
    FRegistryEntityHandle intercept_target;

    BEFORE_EACH()
    {
        spawner =
            ml::level_test_setup(TEXT("FuncT_fighters_intercept_capital"), TestRunner, checks);
    }
    AFTER_EACH()
    {
        if (test_driver.IsSet()) {
            test_driver->orchestrator.clear_end_tick_test_hook();
        }
        test_driver->orchestrator.pause_simulation();
    }
  private:
    void sample_values(ATestBatchOrchestrator&) {
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

    void on_end_tick(ATestBatchOrchestrator & orchestrator) {
        sample_values(orchestrator);
        test_driver->timeline.tick(test_driver->get_time());
    }

    void initial_setup() {
        test_driver = ml::TestSimulationDriver::from_world(spawner->GetWorld());
        test_driver->orchestrator.start_simulation();

        capitals = &test_driver->get_capital_ships();
        fighters = &test_driver->get_capital_ship_fighters();

        hero_capital_index = *capitals->find_first_index_on_team(ETestTeam::Green);
        hero_capital = capitals->get_handle(hero_capital_index);
        original_target = *capitals->find_first_handle_on_team(ETestTeam::Red);
        intercept_target = *capitals->find_first_handle_on_team(ETestTeam::Blue);

        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));
        test_driver->timeline.finish_at(test_duration);
    }

    void check_fighters_share_target(FSimulationSample const& sample, FString const& description) {
        auto const& fighter_targets{sample.fighter_targets};
        if (!checks.is_greater_than(fighter_targets.Num(), int32{0}, description)) {
            return;
        }

        auto const shared_target{fighter_targets[0]};
        for (int32 fighter_index{1}; fighter_index < fighter_targets.Num(); ++fighter_index) {
            checks.are_equal(
                shared_target, fighter_targets[fighter_index], description, fighter_index);
        }
    }

    void full_checks() {
        auto const& start{samples.nearest_value(initial_setup_duration)};
        auto const& end{samples.nearest_value(test_duration)};

        checks.is_greater_than(start.fighter_targets.Num(), int32{0}, TEXT("Parent has fighters"));
        checks.is_greater_than(
            end.fighter_targets.Num(), int32{0}, TEXT("Parent has fighters at end"));
        checks.are_equal(original_target,
                         start.parent_target,
                         TEXT("Green capital initially targets red capital"));

        for (int32 fighter_index{0}; fighter_index < start.fighter_targets.Num(); ++fighter_index) {
            checks.are_equal(original_target,
                             start.fighter_targets[fighter_index],
                             TEXT("Initial fighter target matches red parent target"),
                             fighter_index);
        }

        for (int32 fighter_index{0}; fighter_index < end.fighter_targets.Num(); ++fighter_index) {
            checks.are_equal(intercept_target,
                             end.fighter_targets[fighter_index],
                             TEXT("Final fighter target is blue capital"),
                             fighter_index);
        }

        check_fighters_share_target(start, TEXT("Fighters share their initial target"));
        check_fighters_share_target(end, TEXT("Fighters share their final target"));
    }

    void export_data() const {
        auto const result_asset{
            ml::FTestResultAsset{TEXT("fighters_intercept_capital"), *TestRunner}};
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
        for (int32 sample_index{0}; sample_index < sample_values.Num(); ++sample_index) {
            auto const& sample{sample_values[sample_index]};

            FFightersInterceptCapitalTimeSeriesRow row{};
            row.time = sample_times[sample_index];
            row.parent_target = sample.parent_target.to_string();
            row.fighter_targets.Reserve(sample.fighter_targets.Num());
            for (auto const fighter_target : sample.fighter_targets) {
                row.fighter_targets.Add(fighter_target.to_string());
            }

            curve_times.Add(static_cast<float>(sample_times[sample_index]));
            fighter_counts.Add(sample.fighter_count);
            results->time_series_results.Add(MoveTemp(row));
        }

        ml::add_simple_curve_row(*curves,
                                 TEXT("fighter_count"),
                                 TConstArrayView<int32>{fighter_counts},
                                 TConstArrayView<float>{curve_times});

        result_asset.save(*results);
        result_asset.save(*curves);
    }

    TEST_METHOD(MainTest)
    {
        TestCommandBuilder.StartWhen([this] { return nullptr != spawner->FindFirstPlayerPawn(); })
            .Then([this] { initial_setup(); })
            .Until([this] { return test_driver->timeline.is_finished(); }, FTimespan{0, 0, 11})
            .Then([this] {
                full_checks();
                if (!checks.all_passed || test_driver->should_export_results()) {
                    export_data();
                }
                SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
            });
    }
};
