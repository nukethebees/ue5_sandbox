#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>

#include <SandboxTests/cqtests/level_checks.h>
#include <SandboxTests/cqtests/SoftTestAssertions.h>
#include <SandboxTests/cqtests/test_setup.h>
#include <SandboxTests/cqtests/TestSimulationDriver.h>

#include <SandboxCore/time_series_data.h>

#include <Components/MapTestSpawner.h>
#include <CQTest.h>
#include <Misc/Optional.h>

TEST_CLASS(FighterLosFailureHandling, "Sandbox.FunctionalTests")
{
    using ThisClass = FighterLosFailureHandling;

    struct FSimulationSample {
        TArray<ETestTeam> fighter_teams;
    };

    static constexpr double test_duration{30.0};

    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    ml::FSoftTestAssertions checks{};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};

    FRegistryEntityHandle enemy;
    ETestTeam hero_team{ETestTeam::Blue};
    ETestTeam enemy_team{ETestTeam::Red};
    int32 initial_enemy_health{0};

    ml::TimeSeriesData<FSimulationSample> samples;

    BEFORE_EACH()
    {
        spawner =
            ml::level_test_setup(TEXT("FuncT_fighter_los_failure_handling"), TestRunner, checks);
    }
    AFTER_EACH()
    {
        if (test_driver.IsSet()) {
            test_driver->orchestrator.clear_end_tick_test_hook();
            test_driver->orchestrator.pause_simulation();
        }
    }
  private:
    void sample_values(ATestBatchOrchestrator&) {
        FSimulationSample sample{};
        sample.fighter_teams.Append(test_driver->get_capital_ship_fighters().get_teams());
        samples.add(test_driver->get_time(), MoveTemp(sample));
    }

    void on_end_tick(ATestBatchOrchestrator & orchestrator) {
        sample_values(orchestrator);
        test_driver->timeline.tick(test_driver->get_time());
    }

    void initial_setup() {
        auto& world{spawner->GetWorld()};
        test_driver = ml::TestSimulationDriver::from_world(world);

        auto const maybe_enemy{
            test_driver->get_capital_ships().find_first_handle_on_team(enemy_team)};
        if (checks.is_true(maybe_enemy.has_value(), TEXT("Find enemy capital"))) {
            enemy = *maybe_enemy;
            initial_enemy_health = test_driver->get_capital_ships().get_health(enemy);
        }

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));
        test_driver->timeline.finish_at(test_duration);
        test_driver->orchestrator.start_simulation();
    }

    void check_fighter_spawns_and_survival() {
        TOptional<int32> spawned_fighter_count{NullOpt};

        auto const sample_values{samples.values()};
        auto const sample_count{sample_values.Num()};
        for (int32 sample_index{0}; sample_index < sample_count; ++sample_index) {
            auto const& fighter_teams{sample_values[sample_index].fighter_teams};

            ml::check_all_teams_are(
                fighter_teams, hero_team, checks, TEXT("Only the blue hero team has fighters"));
        }
    }

    void full_checks() {
        ml::check_samples_recorded(samples.num(), checks, TEXT("Simulation produced samples"));
        if (samples.is_empty()) {
            SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
            return;
        }

        check_fighter_spawns_and_survival();
        auto const final_enemy_health{test_driver->get_capital_ships().get_health(enemy)};
        ml::check_health_decreased(
            initial_enemy_health,
            final_enemy_health,
            checks,
            TEXT("Enemy capital has sustained damage by the end of the test"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }

    TEST_METHOD(Main)
    {
        FTimespan const timeout{0, 0, static_cast<int32>(test_duration) + 1};
        TestCommandBuilder.Do([this] { initial_setup(); })
            .Until([this] { return test_driver->timeline.is_finished(); }, timeout)
            .Then([this] { full_checks(); });
    }
};
