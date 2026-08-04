#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestTeam.h>
#include <Sandbox/core/SandboxDeveloperSettings.h>
#include <Sandbox/utilities/enums.h>

#include <SandboxTests/cqtests/SoftTestAssertions.h>
#include <SandboxTests/cqtests/test_setup.h>
#include <SandboxTests/cqtests/TestSimulationDriver.h>

#include <SandboxCore/test_timeline.h>
#include <SandboxCore/time_series_data.h>

#include <Components/MapTestSpawner.h>
#include <Containers/Set.h>
#include <CQTest.h>
#include <EngineUtils.h>
#include <Misc/Optional.h>

TEST_CLASS(CapitalCommandFighters, "Sandbox.FunctionalTests")
{
    using Task = ATestCapitalShipFighters::Task;
    using ThisClass = CapitalCommandFighters;
    using time_type = ml::TestSimulationDriver::time_type;

    struct FSimulationSample {
        FRegistryEntityHandle capital_target;
        TArray<FRegistryEntityHandle> fighter_targets;
        TArray<Task> fighter_tasks;
        int32 capital_count{0};
    };

    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};
    ml::FSoftTestAssertions checks{};

    ATestCapitalShips const* capitals{nullptr};
    ATestCapitalShipFighters const* fighters{nullptr};

    static constexpr time_type wait_after_setup{2.0 / 60.0};
    static constexpr time_type wait_after_kills{2.0 / 60.0};

    FRegistryEntityHandle capital_first_target;
    FRegistryEntityHandle capital_second_target;
    int32 capital_fighter_start{0};
    int32 capital_fighter_end{0};

    ETestTeam team_kept_alive;
    static constexpr int32 test_capital_idx{0};

    ml::TimeSeriesData<FSimulationSample> samples;
    FTestTimeline timeline;
    time_type t_after_setup{0.0};
    time_type t_after_initial_kills{0.0};
    time_type t_after_all_kills{0.0};

    BEFORE_EACH()
    { spawner = ml::level_test_setup(TEXT("FuncT_capital_command_fighters"), TestRunner, checks); }
    AFTER_EACH()
    { test_driver->orchestrator.clear_end_tick_test_hook(); }
  private:
    void sample_values(ATestBatchOrchestrator&) {
        auto const t{test_driver->get_time()};
        FSimulationSample sample{};
        sample.capital_target = capitals->get_target_handle(test_capital_idx);
        sample.fighter_targets.Append(fighters->get_target_handles());
        sample.fighter_tasks.Append(fighters->get_tasks());
        sample.capital_count = capitals->get_num_instances();
        samples.add(t, MoveTemp(sample));
    }
    void on_end_tick(ATestBatchOrchestrator & orchestrator) {
        sample_values(orchestrator);
        timeline.tick(test_driver->get_time());
    }

    void initial_setup() {
        auto& world{spawner->GetWorld()};
        test_driver = ml::TestSimulationDriver::from_world(world);
        test_driver->orchestrator.start_simulation();

        capitals = &test_driver->get_capital_ships();
        fighters = &test_driver->get_capital_ship_fighters();

        team_kept_alive = capitals->get_team(test_capital_idx);
        capital_first_target = capitals->get_target_handle(test_capital_idx);

        auto const capital_fighter_span{
            capitals->get_capital_fighter_handle_span(test_capital_idx)};
        capital_fighter_start = capital_fighter_span.start();
        capital_fighter_end = capital_fighter_span.end();
    }
    void initial_setup_and_stimuli() {
        initial_setup();
        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));

        timeline
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
            .finish_after(wait_after_kills,
                          [this] { t_after_all_kills = test_driver->get_time(); });
    }

    template <auto EnumValue>
    void check_fighter_tasks_are(FSimulationSample const& sample) {
        auto const& tasks{sample.fighter_tasks};
        auto const n{tasks.Num()};

        for (int32 i{0}; i < n; ++i) {
            checks.are_equal(EnumValue,
                             tasks[i],
                             FString::Printf(TEXT("Check fighter %d is in %s"),
                                             i,
                                             *ml::to_string_without_type_prefix(EnumValue)));
        }
    }

    void check_target_handles(FRegistryEntityHandle const capital_target,
                              FSimulationSample const& sample) {
        checks.are_equal(
            capital_target, sample.capital_target, TEXT("Capital handle hasn't changed"));

        auto const& fighter_targets{sample.fighter_targets};

        for (int32 i{capital_fighter_start}; i < capital_fighter_end; ++i) {
            checks.are_equal(capital_target,
                             fighter_targets[i],
                             FString::Printf(TEXT("Fighter target handles [%d]"), i));
        }
    }

    void kill_initial_targets() {
        test_driver->queue_kills(TArray{capitals->get_target_handle(test_capital_idx)});
    }
    void full_checks() {
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

    void kill_all_not_on_main_team() {
        auto const handles{test_driver->registry.get_handles_not_in_team(team_kept_alive)};
        test_driver->queue_kills(handles);
    }

    TEST_METHOD(MainTest)
    {
        TestCommandBuilder.StartWhen([this] { return nullptr != spawner->FindFirstPlayerPawn(); })
            .Then([this] { initial_setup_and_stimuli(); })
            .Until([this] { return timeline.is_finished(); }, FTimespan{0, 0, 1})
            .Then([this] { full_checks(); });
    }
};
