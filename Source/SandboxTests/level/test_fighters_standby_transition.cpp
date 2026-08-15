#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>
#include <Sandbox/batch_game/TestTeam.h>

#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestSimulationDriver.h>
#include <SandboxTests/support/level_checks.h>
#include <SandboxTests/support/test_setup.h>

#include <SandboxCore/time_series_data.h>

#include <Components/MapTestSpawner.h>
#include <CQTest.h>
#include <Engine/World.h>
#include <Misc/Optional.h>

TEST_CLASS(FightersStandbyTransition, "Sandbox.LevelTests")
{
    using ThisClass = FightersStandbyTransition;
    using Task = ATestCapitalShipFighters::Task;
    using time_type = ml::TestSimulationDriver::time_type;

    struct FSimulationSample {
        int32 capital_count{0};
        TArray<FRegistryEntityHandle> fighter_handles;
        TArray<Task> fighter_tasks;
        TArray<FVector3f> fighter_velocities;
    };

    static constexpr time_type pre_kill_wait{8.0};
    static constexpr time_type post_kill_wait{0.1};
    inline static FTimespan const timeout{0, 0, 2};

    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    TOptional<ml::FTestBatchOrchestratorLevelSetup> level_setup{NullOpt};
    ml::FSoftTestAssertions checks{};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};

    FRegistryEntityHandle enemy_capital;
    ml::TimeSeriesData<FSimulationSample> samples;
    TOptional<time_type> pre_kill_time{NullOpt};
    TOptional<time_type> post_kill_time{NullOpt};

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        enemy_capital = {};
        samples = {};
        pre_kill_time.Reset();
        post_kill_time.Reset();

        spawner = FMapTestSpawner::CreateFromTempLevel(TestCommandBuilder);
        level_setup.Emplace(*spawner, *TestRunner, checks);
        level_setup->setup(
            TestCommandBuilder,
            [this](UWorld& world, UTestSimulationConfig const& config) { spawn_capitals(world, config); });
    }
    AFTER_EACH()
    {
        if (test_driver.IsSet()) {
            test_driver->orchestrator.clear_end_tick_test_hook();
        }

        level_setup->teardown();
        level_setup.Reset();
        spawner.Reset();
    }
  private:
    /* ------------------------------------------------------------------------------------------ */
    // Setup
    /* ------------------------------------------------------------------------------------------ */
    void spawn_capitals(UWorld & world, UTestSimulationConfig const& config) {
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

    void initial_setup() {
        test_driver = ml::TestSimulationDriver::from_world(level_setup->get_world());

        auto const& capitals{test_driver->get_capital_ships()};
        checks.are_equal(2, capitals.get_num_instances(), TEXT("Two capitals are registered"));

        auto const enemy_index{capitals.find_first_index_on_team(ETestTeam::Red)};
        if (checks.is_true(enemy_index.has_value(), TEXT("Find enemy capital"))) {
            enemy_capital = capitals.get_handle(*enemy_index);
        }

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));
        test_driver->timeline
            .then_after(pre_kill_wait, [this] {
                pre_kill_time = test_driver->get_time();
                test_driver->queue_kills(TArray{enemy_capital});
            })
            .then_after(post_kill_wait, [this] { post_kill_time = test_driver->get_time(); })
            .finish_after(0.0);
        test_driver->orchestrator.start_simulation();
    }

    void on_end_tick(ATestBatchOrchestrator&) {
        sample_values();
        test_driver->timeline.tick(test_driver->get_time());
    }

    /* ------------------------------------------------------------------------------------------ */
    // Samples
    /* ------------------------------------------------------------------------------------------ */
    void sample_values() {
        auto const& capitals{test_driver->get_capital_ships()};
        auto const& fighters{test_driver->get_capital_ship_fighters()};
        auto const fighter_handles{fighters.get_handles()};

        FSimulationSample sample{};
        sample.capital_count = capitals.get_num_instances();
        sample.fighter_handles.Append(fighter_handles);
        sample.fighter_tasks.Append(fighters.get_tasks());
        sample.fighter_velocities.Reserve(fighter_handles.Num());
        for (auto const fighter_handle : fighter_handles) {
            sample.fighter_velocities.Add(test_driver->registry.get_velocity(fighter_handle));
        }

        samples.add(test_driver->get_time(), MoveTemp(sample));
    }

    /* ------------------------------------------------------------------------------------------ */
    // Checks
    /* ------------------------------------------------------------------------------------------ */
    void check_pre_kill_state(FSimulationSample const& sample) {
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

    void check_post_kill_state(FSimulationSample const& sample) {
        checks.are_equal(1, sample.capital_count, TEXT("One capital remains after kill"));
        if (!checks.is_greater_than(sample.fighter_handles.Num(),
                                    int32{0},
                                    TEXT("Fighters remain after kill"))) {
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
            checks.are_equal(Task::Standby,
                             sample.fighter_tasks[i],
                             TEXT("Fighter transitioned to standby"),
                             i);
            checks.dist_zero(sample.fighter_velocities[i],
                             FVector3f::ZeroVector,
                             0.0f,
                             TEXT("Registry fighter velocity is zero in standby"),
                             i);
        }
    }

    void full_checks() {
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

    TEST_METHOD(Main)
    {
        TestCommandBuilder.Do([this] { initial_setup(); })
            .Until([this] { return test_driver->timeline.is_finished(); }, timeout)
            .Then([this] { full_checks(); });
    }
};
