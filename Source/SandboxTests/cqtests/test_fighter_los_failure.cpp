#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>

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
        int32 enemy_health{0};
        TArray<ETestTeam> fighter_teams;
    };

    static constexpr double test_duration{20.0};
    inline static FTimespan const default_timeout{0, 0, 12};

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
        sample.enemy_health = test_driver->get_capital_ships().get_health(enemy);
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
            auto const fighter_count{fighter_teams.Num()};

            if (fighter_count == 0) {
                checks.is_true(!spawned_fighter_count.IsSet(),
                               TEXT("Fighter count does not return to zero after spawning"),
                               sample_index);
                continue;
            }

            if (!spawned_fighter_count.IsSet()) {
                spawned_fighter_count = fighter_count;
            } else {
                checks.are_equal(*spawned_fighter_count,
                                 fighter_count,
                                 TEXT("Fighter count remains unchanged after the single spawn"),
                                 sample_index);
            }

            for (int32 fighter_index{0}; fighter_index < fighter_count; ++fighter_index) {
                checks.are_equal(hero_team,
                                 fighter_teams[fighter_index],
                                 TEXT("Only the blue hero team has fighters"),
                                 fighter_index);
            }
        }

        checks.is_true(spawned_fighter_count.IsSet(), TEXT("Hero fighters spawned"));
        if (spawned_fighter_count.IsSet()) {
            checks.is_greater_than(
                *spawned_fighter_count, int32{0}, TEXT("Spawned fighter count is non-zero"));
        }
    }

    void full_checks() {
        checks.is_greater_than(samples.num(), int32{0}, TEXT("Simulation produced samples"));
        if (samples.is_empty()) {
            SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
            return;
        }

        check_fighter_spawns_and_survival();
        checks.is_true(samples.last_value().enemy_health < initial_enemy_health,
                       TEXT("Fighters recover from blocked LOS and damage the enemy capital"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }

    TEST_METHOD(Main)
    {
        TestCommandBuilder.Do([this] { initial_setup(); })
            .Until([this] { return test_driver->timeline.is_finished(); }, default_timeout)
            .Then([this] { full_checks(); });
    }
};
