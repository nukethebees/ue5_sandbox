#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestTeam.h>

#include <SandboxTests/cqtests/SoftTestAssertions.h>
#include <SandboxTests/cqtests/test_setup.h>
#include <SandboxTests/cqtests/TestResultAssetIO.h>
#include <SandboxTests/cqtests/TestSimulationDriver.h>

#include <SandboxCore/time_series_data.h>

#include <Components/MapTestSpawner.h>
#include <Containers/Set.h>
#include <CQTest.h>
#include <Misc/Optional.h>

/*
This test relies on a long spawn delay to ensure more fighters are not spawned.
The assumption is that there is one wave of fighters total.
*/

TEST_CLASS(CapitalFighterHandles, "Sandbox.FunctionalTests")
{
    using ThisClass = CapitalFighterHandles;
    using Task = ATestCapitalShipFighters::Task;
    using time_type = ml::TestSimulationDriver::time_type;

    struct FCapitalSample {
        FRegistryEntityHandle handle;
        FRegistryEntityHandle target_handle;
    };

    struct FFighterSample {
        FRegistryEntityHandle handle{};
        FRegistryEntityHandle target_handle{};
        FVector3f target_location{FVector3f::ZeroVector};
    };

    struct FSimulationSnapshot {
        int32 fighter_spawn_slots{0};
        int32 fighter_count{0};
        TArray<FCapitalSample> capitals;
        TArray<FRegistryEntityHandle> capital_fighter_handles;
        TArray<int32> capital_fighter_counts;
        TArray<FRegistryEntityHandle> fighter_handles;
        TArray<FRegistryEntityHandle> fighter_target_handles;
        TArray<Task> fighter_tasks;
        TArray<FFighterSample> main_capital_fighters;
    };

    static constexpr ETestTeam main_capital_team{ETestTeam::Green};
    static constexpr int32 n_capitals_exp{3};

    static constexpr time_type initial_sample_delay{2.0};
    static constexpr time_type fighter_kill_delay{3.0};
    static constexpr time_type post_fighter_kill_sample_delay{2.0};
    static constexpr time_type capital_kill_delay{3.0};
    static constexpr time_type post_capital_kill_sample_delay{5.0};
    static constexpr time_type final_sample_delay{5.0};

    inline static FTimespan const default_timeout{0, 0, 5};

    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    ml::FSoftTestAssertions checks{};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};

    ATestCapitalShips const* capitals{nullptr};
    ATestCapitalShipFighters const* fighters{nullptr};
    ml::TimeSeriesData<ATestBatchOrchestrator::tick_type> orchestrator_tick_samples;
    ml::TimeSeriesData<int32> fighter_spawn_slots_samples;
    ml::TimeSeriesData<int32> fighter_count_samples;
    ml::TimeSeriesData<int32> capital_count_samples;
    ml::TimeSeriesData<int32> capital_fighter_handle_count_samples;
    ml::TimeSeriesData<int32> capital_fighter_span_count_samples;
    ml::TimeSeriesData<int32> fighter_handle_count_samples;
    ml::TimeSeriesData<int32> fighter_target_handle_count_samples;
    ml::TimeSeriesData<int32> fighter_task_count_samples;
    ml::TimeSeriesData<int32> main_capital_fighter_count_samples;

    ml::TimeSeriesData<TArray<FCapitalSample>> capital_samples;
    ml::TimeSeriesData<TArray<FRegistryEntityHandle>> capital_fighter_handle_samples;
    ml::TimeSeriesData<TArray<int32>> capital_fighter_count_samples;
    ml::TimeSeriesData<TArray<FRegistryEntityHandle>> fighter_handle_samples;
    ml::TimeSeriesData<TArray<FRegistryEntityHandle>> fighter_target_handle_samples;
    ml::TimeSeriesData<TArray<Task>> fighter_task_samples;
    ml::TimeSeriesData<TArray<FFighterSample>> main_capital_fighter_samples;

    FRegistryEntityHandle main_capital_handle;
    TArray<FRegistryEntityHandle> destroyed;
    TArray<FRegistryEntityHandle> kept;

    TOptional<time_type> t_initial;
    TOptional<time_type> t_post_fighter_kill;
    TOptional<time_type> t_pre_capital_kill;
    TOptional<time_type> t_post_capital_kill;

    BEFORE_EACH()
    { spawner = ml::level_test_setup(TEXT("FuncT_capital_fighter_handles"), TestRunner, checks); }
    AFTER_EACH()
    {
        if (test_driver.IsSet()) { test_driver->orchestrator.clear_end_tick_test_hook(); }
    }
  private:
    void sample_values(ATestBatchOrchestrator& orchestrator) {
        auto const fighter_spawn_slots{capitals->get_fighter_spawn_slots()};
        auto const fighter_count{fighters->get_num_instances()};
        TArray<FRegistryEntityHandle> capital_fighter_handles;
        TArray<FRegistryEntityHandle> fighter_handles;
        TArray<FRegistryEntityHandle> fighter_target_handles;
        TArray<Task> fighter_tasks;
        capital_fighter_handles.Append(capitals->get_fighter_handles());
        fighter_handles.Append(fighters->get_handles());
        fighter_target_handles.Append(fighters->get_target_handles());
        fighter_tasks.Append(fighters->get_tasks());

        auto const n_capitals{capitals->get_num_instances()};
        TArray<FCapitalSample> capital_values;
        TArray<int32> capital_fighter_counts;
        TArray<FFighterSample> main_capital_fighters;
        capital_values.Reserve(n_capitals);
        capital_fighter_counts.Reserve(n_capitals);

        for (int32 capital_index{0}; capital_index < n_capitals; ++capital_index) {
            auto const capital_handle{capitals->get_handle(capital_index)};
            auto const capital_target{capitals->get_target_handle(capital_index)};
            auto const fighter_span{capitals->get_capital_fighter_handle_span(capital_index)};

            capital_values.Add(FCapitalSample{capital_handle, capital_target});
            capital_fighter_counts.Add(fighter_span.count);

            if (capital_handle != main_capital_handle) { continue; }

            auto const main_fighter_handles{capitals->get_fighter_handles(capital_index)};
            main_capital_fighters.Reserve(main_fighter_handles.Num());

            for (auto const fighter_handle : main_fighter_handles) {
                if (fighters->has_handle(fighter_handle)) {
                    main_capital_fighters.Emplace(
                        fighter_handle,
                        fighters->get_target_handle(fighter_handle),
                        fighters->get_target_location(fighter_handle));
                } else {
                    main_capital_fighters.Emplace();
                }
            }
        }

        auto const time{test_driver->get_time()};
        orchestrator_tick_samples.add(time, orchestrator.get_completed_ticks());
        fighter_spawn_slots_samples.add(time, fighter_spawn_slots);
        fighter_count_samples.add(time, fighter_count);
        capital_count_samples.add(time, capital_values.Num());
        capital_fighter_handle_count_samples.add(time, capital_fighter_handles.Num());
        capital_fighter_span_count_samples.add(time, capital_fighter_counts.Num());
        fighter_handle_count_samples.add(time, fighter_handles.Num());
        fighter_target_handle_count_samples.add(time, fighter_target_handles.Num());
        fighter_task_count_samples.add(time, fighter_tasks.Num());
        main_capital_fighter_count_samples.add(time, main_capital_fighters.Num());

        capital_samples.add(time, MoveTemp(capital_values));
        capital_fighter_handle_samples.add(time, MoveTemp(capital_fighter_handles));
        capital_fighter_count_samples.add(time, MoveTemp(capital_fighter_counts));
        fighter_handle_samples.add(time, MoveTemp(fighter_handles));
        fighter_target_handle_samples.add(time, MoveTemp(fighter_target_handles));
        fighter_task_samples.add(time, MoveTemp(fighter_tasks));
        main_capital_fighter_samples.add(time, MoveTemp(main_capital_fighters));
    }

    void on_end_tick(ATestBatchOrchestrator & orchestrator) {
        sample_values(orchestrator);
        test_driver->timeline.tick(test_driver->get_time());
    }

    auto find_main_capital_index() const -> TOptional<int32> {
        auto const n_capitals{capitals->get_num_instances()};
        for (int32 capital_index{0}; capital_index < n_capitals; ++capital_index) {
            if (capitals->get_handle(capital_index) == main_capital_handle) {
                return capital_index;
            }
        }

        return NullOpt;
    }

    void initial_setup() {
        test_driver = ml::TestSimulationDriver::from_world(spawner->GetWorld());
        checks.are_equal(ATestBatchOrchestrator::tick_type{0},
                         test_driver->orchestrator.get_completed_ticks(),
                         TEXT("Simulation is paused before the test starts it"));
        test_driver->orchestrator.start_simulation();

        capitals = &test_driver->get_capital_ships();
        fighters = &test_driver->get_capital_ship_fighters();

        auto const main_capital_index{capitals->find_first_index_on_team(main_capital_team)};
        if (checks.is_true(main_capital_index.has_value(), TEXT("Find green main capital"))) {
            main_capital_handle = capitals->get_handle(*main_capital_index);
        }

        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));
    }

    void kill_fighters() {
        destroyed.Reset();
        kept.Reset();

        auto const fighter_handles{capitals->get_fighter_handles()};
        for (int32 fighter_index{0}; fighter_index < fighter_handles.Num(); ++fighter_index) {
            auto const handle{fighter_handles[fighter_index]};
            if (fighter_index % 2 == 0) {
                destroyed.Add(handle);
            } else {
                kept.Add(handle);
            }
        }

        test_driver->queue_kills(destroyed);
    }

    void kill_capital_opponent() {
        auto const main_capital_index{find_main_capital_index()};
        if (!checks.is_true(main_capital_index.IsSet(), TEXT("Find main capital before kill"))) {
            return;
        }

        auto const target{capitals->get_target_handle(*main_capital_index)};
        if (!checks.is_true(test_driver->registry.is_valid_alive(target),
                            TEXT("Capital target is alive before kill"))) {
            return;
        }

        test_driver->queue_kills(TArray{target});
    }

    void configure_timeline(bool const should_kill_fighters, bool const should_kill_capital) {
        test_driver->timeline.then_after(initial_sample_delay,
                                         [this] { t_initial = test_driver->get_time(); });

        if (should_kill_fighters) {
            test_driver->timeline.then_after(fighter_kill_delay, [this] { kill_fighters(); });
            test_driver->timeline.then_after(post_fighter_kill_sample_delay, [this] {
                t_post_fighter_kill = test_driver->get_time();
            });
        }

        if (should_kill_capital) {
            test_driver->timeline.then_after(capital_kill_delay, [this] {
                t_pre_capital_kill = test_driver->get_time();
                kill_capital_opponent();
            });
            test_driver->timeline.then_after(post_capital_kill_sample_delay, [this] {
                t_post_capital_kill = test_driver->get_time();
            });
        }

        test_driver->timeline.finish_after(final_sample_delay);
    }

    void check_capitals_not_targeting_self(FSimulationSnapshot const& sample) {
        for (int32 capital_index{0}; capital_index < sample.capitals.Num(); ++capital_index) {
            auto const& capital{sample.capitals[capital_index]};
            checks.not_equal(capital.handle,
                             capital.target_handle,
                             TEXT("Capital is not targeting itself"),
                             capital_index);
        }
    }

    void check_main_capital_fighters_not_targeting_parent(FSimulationSnapshot const& sample,
                                                          FString const& description) {
        for (int32 fighter_index{0}; fighter_index < sample.main_capital_fighters.Num();
             ++fighter_index) {
            checks.not_equal(main_capital_handle,
                             sample.main_capital_fighters[fighter_index].target_handle,
                             description,
                             fighter_index);
        }
    }

    void check_spawn_capital_handle_state(FSimulationSnapshot const& sample) {
        checks.are_equal(n_capitals_exp, sample.capitals.Num(), TEXT("Number of capitals"));

        auto const expected_fighter_count{sample.capitals.Num() * sample.fighter_spawn_slots};
        checks.are_equal(
            expected_fighter_count, sample.fighter_count, TEXT("Number of spawned fighters"));
        checks.are_equal(expected_fighter_count,
                         sample.capital_fighter_handles.Num(),
                         TEXT("Number of capital fighter handles"));
        checks.are_equal(sample.capitals.Num(),
                         sample.capital_fighter_counts.Num(),
                         TEXT("Number of capital fighter spans"));
        for (int32 capital_index{0}; capital_index < sample.capital_fighter_counts.Num();
             ++capital_index) {
            checks.are_equal(sample.fighter_spawn_slots,
                             sample.capital_fighter_counts[capital_index],
                             TEXT("Capital fighter span count"),
                             capital_index);
        }

        TSet<FRegistryEntityHandle> unique_handles;
        for (int32 fighter_index{0}; fighter_index < sample.capital_fighter_handles.Num();
             ++fighter_index) {
            auto const handle{sample.capital_fighter_handles[fighter_index]};
            checks.is_true(!unique_handles.Contains(handle),
                           TEXT("Capital fighter handle is unique"),
                           fighter_index);
            unique_handles.Add(handle);
        }
    }

    void check_all_fighter_targets_not_null(FSimulationSnapshot const& sample) {
        for (int32 fighter_index{0}; fighter_index < sample.fighter_target_handles.Num();
             ++fighter_index) {
            checks.is_true(!sample.fighter_target_handles[fighter_index].is_null(),
                           TEXT("Fighter target is not null"),
                           fighter_index);
        }
    }

    void check_fighter_kill_state(FSimulationSnapshot const& initial,
                                  FSimulationSnapshot const& after_kill) {
        for (int32 fighter_index{0}; fighter_index < after_kill.capital_fighter_handles.Num();
             ++fighter_index) {
            auto const handle{after_kill.capital_fighter_handles[fighter_index]};
            checks.is_true(
                kept.Contains(handle), TEXT("Remaining fighter handle was kept"), fighter_index);
            checks.is_true(!destroyed.Contains(handle),
                           TEXT("Remaining fighter handle was not destroyed"),
                           fighter_index);
        }

        for (int32 fighter_index{0}; fighter_index < kept.Num(); ++fighter_index) {
            checks.is_true(test_driver->registry.is_valid_alive(kept[fighter_index]),
                           TEXT("Kept fighter is alive"),
                           fighter_index);
        }
        for (int32 fighter_index{0}; fighter_index < destroyed.Num(); ++fighter_index) {
            checks.is_true(test_driver->registry.is_valid_dead(destroyed[fighter_index]),
                           TEXT("Destroyed fighter is dead"),
                           fighter_index);
        }

        checks.are_equal(kept.Num(),
                         after_kill.capital_fighter_handles.Num(),
                         TEXT("Expected number of remaining fighters"));

        for (int32 fighter_index{0}; fighter_index < after_kill.main_capital_fighters.Num();
             ++fighter_index) {
            auto const handle{after_kill.main_capital_fighters[fighter_index].handle};
            auto const was_initially_parented{initial.main_capital_fighters.ContainsByPredicate(
                [handle](FFighterSample const& fighter) { return fighter.handle == handle; })};
            checks.is_true(was_initially_parented,
                           TEXT("Remaining main-capital fighter retains its parent"),
                           fighter_index);
        }
    }

    void check_main_capital_fighter_handles_unchanged(FSimulationSnapshot const& before,
                                                      FSimulationSnapshot const& after) {
        if (!checks.are_equal(before.main_capital_fighters.Num(),
                              after.main_capital_fighters.Num(),
                              TEXT("Number of main-capital fighters is unchanged"))) {
            return;
        }

        for (int32 fighter_index{0}; fighter_index < after.main_capital_fighters.Num();
             ++fighter_index) {
            auto const handle{after.main_capital_fighters[fighter_index].handle};
            auto const was_present_before{before.main_capital_fighters.ContainsByPredicate(
                [handle](FFighterSample const& fighter) { return fighter.handle == handle; })};
            checks.is_true(was_present_before,
                           TEXT("Main-capital fighter handle is unchanged"),
                           fighter_index);
        }
    }

    void check_fighter_targets_changed(FSimulationSnapshot const& before,
                                       FSimulationSnapshot const& after) {
        if (!checks.are_equal(
                before.main_capital_fighters.Num(),
                after.main_capital_fighters.Num(),
                TEXT("Number of main-capital fighters before and after capital kill"))) {
            return;
        }

        for (int32 fighter_index{0}; fighter_index < before.main_capital_fighters.Num();
             ++fighter_index) {
            auto const& before_fighter{before.main_capital_fighters[fighter_index]};
            auto const* after_fighter{after.main_capital_fighters.FindByPredicate(
                [&before_fighter](FFighterSample const& fighter) {
                    return fighter.handle == before_fighter.handle;
                })};

            if (!checks.is_true(after_fighter != nullptr,
                                TEXT("Find main-capital fighter after capital kill"),
                                fighter_index)) {
                continue;
            }

            checks.not_equal(before_fighter.target_handle,
                             after_fighter->target_handle,
                             TEXT("Fighter target handle changed after capital kill"),
                             fighter_index);
            checks.not_equal(before_fighter.target_location,
                             after_fighter->target_location,
                             TEXT("Fighter target location changed after capital kill"),
                             fighter_index);
        }
    }

    void check_capital_kill_state(FSimulationSnapshot const& before_kill,
                                  FSimulationSnapshot const& after_kill) {
        checks.are_equal(
            n_capitals_exp - 1, after_kill.capitals.Num(), TEXT("Number of capitals after kill"));
        checks.are_equal(before_kill.fighter_count,
                         after_kill.fighter_count,
                         TEXT("Number of fighters is unchanged after capital kill"));

        check_capitals_not_targeting_self(after_kill);
        check_main_capital_fighters_not_targeting_parent(
            after_kill,
            TEXT("Main-capital fighter is not targeting its parent after capital kill"));
        check_main_capital_fighter_handles_unchanged(before_kill, after_kill);
        check_fighter_targets_changed(before_kill, after_kill);
        check_all_fighter_targets_not_null(after_kill);

        for (int32 fighter_index{0}; fighter_index < after_kill.fighter_tasks.Num();
             ++fighter_index) {
            checks.are_equal(Task::Attack,
                             after_kill.fighter_tasks[fighter_index],
                             TEXT("Fighter is attacking after capital kill"),
                             fighter_index);
        }
    }

    auto snapshot_at(time_type const time) const -> FSimulationSnapshot {
        auto const sample_index{fighter_spawn_slots_samples.nearest_index(time)};
        check(sample_index != INDEX_NONE);

        return FSimulationSnapshot{
            fighter_spawn_slots_samples.value_at(sample_index),
            fighter_count_samples.value_at(sample_index),
            capital_samples.value_at(sample_index),
            capital_fighter_handle_samples.value_at(sample_index),
            capital_fighter_count_samples.value_at(sample_index),
            fighter_handle_samples.value_at(sample_index),
            fighter_target_handle_samples.value_at(sample_index),
            fighter_task_samples.value_at(sample_index),
            main_capital_fighter_samples.value_at(sample_index)};
    }

    void check_fighter_handle_counts_for_all_ticks() {
        auto const fighter_handle_counts{fighter_handle_count_samples.values()};
        auto const capital_fighter_handle_counts{capital_fighter_handle_count_samples.values()};
        auto const ticks{orchestrator_tick_samples.values()};
        check(fighter_handle_counts.Num() == capital_fighter_handle_counts.Num());
        check(fighter_handle_counts.Num() == ticks.Num());

        TArray<ATestBatchOrchestrator::tick_type> mismatch_ticks;
        bool has_consecutive_mismatches{false};
        for (int32 sample_index{0}; sample_index < fighter_handle_counts.Num(); ++sample_index) {
            if (fighter_handle_counts[sample_index] ==
                capital_fighter_handle_counts[sample_index]) {
                continue;
            }

            auto const tick{ticks[sample_index]};
            if (!mismatch_ticks.IsEmpty() && tick == mismatch_ticks.Last() + 1) {
                has_consecutive_mismatches = true;
            }
            mismatch_ticks.Add(tick);
        }

        TestRunner->AddInfo(FString::Printf(TEXT("Fighter handle count mismatch ticks: %d"),
                                            mismatch_ticks.Num()));
        if (!has_consecutive_mismatches) { return; }

        FString mismatch_tick_list;
        for (auto const tick : mismatch_ticks) {
            if (!mismatch_tick_list.IsEmpty()) { mismatch_tick_list += TEXT(", "); }
            mismatch_tick_list += LexToString(tick);
        }
        checks.is_true(false,
                       FString::Printf(
                           TEXT("Fighter and capital-owned fighter handle counts differ on "
                                "consecutive ticks. All mismatch ticks: %s"),
                           *mismatch_tick_list));
    }

    void full_checks(bool const should_kill_fighters, bool const should_kill_capital) {
        check_fighter_handle_counts_for_all_ticks();

        check(t_initial.IsSet());
        auto const initial{snapshot_at(*t_initial)};

        check_spawn_capital_handle_state(initial);
        check_capitals_not_targeting_self(initial);
        checks.is_greater_than(
            initial.main_capital_fighters.Num(), int32{0}, TEXT("Green capital has fighters"));
        check_main_capital_fighters_not_targeting_parent(
            initial, TEXT("Main-capital fighter is not targeting its parent initially"));
        check_all_fighter_targets_not_null(initial);

        if (should_kill_fighters) {
            check(t_post_fighter_kill.IsSet());
            auto const after_fighter_kill{snapshot_at(*t_post_fighter_kill)};
            check_fighter_kill_state(initial, after_fighter_kill);
            check_main_capital_fighters_not_targeting_parent(
                after_fighter_kill,
                TEXT("Main-capital fighter is not targeting its parent after fighter kill"));
            check_all_fighter_targets_not_null(after_fighter_kill);
        }

        if (should_kill_capital) {
            check(t_pre_capital_kill.IsSet());
            check(t_post_capital_kill.IsSet());

            auto const before_capital_kill{snapshot_at(*t_pre_capital_kill)};
            auto const after_capital_kill{snapshot_at(*t_post_capital_kill)};
            check_capital_kill_state(before_capital_kill, after_capital_kill);
        }
    }

    void export_data(FName const test_name) const {
        auto const result_asset{ml::FTestResultAsset{test_name, *TestRunner}};
        auto* curves{result_asset.load_or_create<UCurveTable>(TEXT("data_curve"))};
        curves->EmptyTable();

        auto add_curve{[curves]<typename T>(FName const name,
                                           ml::TimeSeriesData<T> const& output) {
            ml::add_simple_curve_row(*curves, name, output.values(), output.times());
        }};
        add_curve(TEXT("orchestrator_tick"), orchestrator_tick_samples);
        add_curve(TEXT("fighter_spawn_slots"), fighter_spawn_slots_samples);
        add_curve(TEXT("fighter_count"), fighter_count_samples);
        add_curve(TEXT("capital_count"), capital_count_samples);
        add_curve(TEXT("capital_fighter_handle_count"), capital_fighter_handle_count_samples);
        add_curve(TEXT("capital_fighter_span_count"), capital_fighter_span_count_samples);
        add_curve(TEXT("fighter_handle_count"), fighter_handle_count_samples);
        add_curve(TEXT("fighter_target_handle_count"), fighter_target_handle_count_samples);
        add_curve(TEXT("fighter_task_count"), fighter_task_count_samples);
        add_curve(TEXT("main_capital_fighter_count"), main_capital_fighter_count_samples);

        result_asset.save(*curves);
    }

    void run_test(
        FName const test_name, bool const should_kill_fighters, bool const should_kill_capital) {
        TestCommandBuilder.StartWhen([this] { return nullptr != spawner->FindFirstPlayerPawn(); })
            .Then([this, should_kill_fighters, should_kill_capital] {
                initial_setup();
                configure_timeline(should_kill_fighters, should_kill_capital);
            })
            .Until([this] { return test_driver->timeline.is_finished(); }, default_timeout)
            .Then([this, test_name, should_kill_fighters, should_kill_capital] {
                full_checks(should_kill_fighters, should_kill_capital);
                if (!checks.all_passed || test_driver->should_export_results()) {
                    export_data(test_name);
                }
                SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
            });
    }

    TEST_METHOD(KillFightersOnly)
    { run_test(TEXT("capital_fighter_handles_kill_fighters_only"), true, false); }

    TEST_METHOD(KillCapital)
    { run_test(TEXT("capital_fighter_handles_kill_capital"), false, true); }

    TEST_METHOD(All)
    { run_test(TEXT("capital_fighter_handles_all"), true, true); }
};
