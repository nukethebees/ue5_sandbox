#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/core/SandboxDeveloperSettings.h>

#include <SandboxCore/soa_vector_utils.h>

#include <SandboxTests/cqtests/SoftTestAssertions.h>
#include <SandboxTests/cqtests/TestSimulationDriver.h>

#include <Components/MapTestSpawner.h>
#include <Containers/Set.h>
#include <CQTest.h>
#include <EngineUtils.h>
#include <Misc/Optional.h>

namespace {
auto LexToString(FVector3f const& vec) -> FString {
    return vec.ToCompactString();
}
}

TEST_CLASS(CapitalFighterHandles, "Sandbox.FunctionalTests")
{
    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    ml::FSoftTestAssertions<std::remove_cvref_t<decltype(*TestRunner)>> checks{};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};

    ATestCapitalShips const* capitals{nullptr};
    ATestCapitalShipFighters const* fighters{nullptr};

    static constexpr ETestTeam main_capital_team{ETestTeam::Green};

    static constexpr int32 cycles_to_wait{5};
    static constexpr int32 n_capitals_exp{3};

    struct FighterSample {
        TArray<FRegistryEntityHandle> handles;
        TArray<FRegistryEntityHandle> target_handles;
        TArray<FVector3f> target_locations;
    };

    BEFORE_EACH()
    {
        spawner = MakeUnique<FMapTestSpawner>(
            TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/test_levels/functional_tests"),
            TEXT("FuncT_capital_fighter_handles"));
        spawner->AddWaitUntilLoadedCommand(TestRunner);

        checks.test_runner = TestRunner;
        checks.all_passed = true;
#if WITH_EDITOR
        auto const* settings{GetDefault<USandboxDeveloperSettings>()};
        checks.log_successful_assertions = settings->log_successful_assertions;
#endif
    }
  private:
    /* ------------------------------------------------------------------------------------------ */
    // Sampling
    /* ------------------------------------------------------------------------------------------ */
    void fill_fighter_target_info(FighterSample & sample) {
        TestRunner->AddInfo(TEXT("fn: fill_fighter_target_info"));

        auto const main_fighters_span{
            capitals->get_capital_fighter_handle_span(main_capital_index)};
        auto const fighter_target_handles{fighters->get_target_handles()};
        auto const fighter_target_locations{fighters->get_target_locations()};

        auto const span_end{main_fighters_span.end()};
        for (int32 i{main_fighters_span.start()}; i < span_end; ++i) {
            auto const handle{sample.handles[i]};
            sample.target_locations.Add(fighters->get_target_location(handle));
            sample.target_handles.Add(fighters->get_target_handle(handle));
        }
    }

    void save_fighter_handles(TArray<FRegistryEntityHandle> & handles) {
        auto const main_span{capitals->get_capital_fighter_handle_span(main_capital_index)};
        auto const fighter_handles{capitals->get_fighter_handles()};

        for (int32 i{main_span.start()}; i < main_span.end(); ++i) {
            handles.Add(fighter_handles[i]);
        }

        handles.Sort();
    }
    void sample_fighter_data(FighterSample & sample) {
        TestRunner->AddInfo(TEXT("fn: sample_fighter_data"));

        save_fighter_handles(sample.handles);
        fill_fighter_target_info(sample);
    }

    /* ------------------------------------------------------------------------------------------ */
    // General checks
    /* ------------------------------------------------------------------------------------------ */
    void check_capitals_not_targeting_self() {
        TestRunner->AddInfo(TEXT("fn: check_capitals_not_targeting_self"));

        auto const n{capitals->get_num_instances()};

        for (int32 i{}; i < n; ++i) {
            auto const handle{capitals->get_handle(i)};
            auto const target{capitals->get_target_handle(i)};

            checks.not_equal(
                handle, target, FString::Printf(TEXT("Check capital[%d] not targeting self"), i));
        }
    }

    /* ------------------------------------------------------------------------------------------ */
    // Logging
    /* ------------------------------------------------------------------------------------------ */
    void log_capital_fighter_info(FighterSample const& sample, FString const info) {
        auto msg{FString::Printf(TEXT("Capital fighters (%s):"), *info)};

        msg += TEXT("\n    Handles, Targets, Target locs");

        auto const n{sample.handles.Num()};
        for (int32 i{0}; i < n; ++i) {
            msg += FString::Printf(TEXT("\n    [%d] %s, %s, %s"),
                                   i,
                                   *sample.handles[i].to_string(),
                                   *sample.target_handles[i].to_string(),
                                   *sample.target_locations[i].ToCompactString());
        }

        TestRunner->AddInfo(msg);
    }

    /* ------------------------------------------------------------------------------------------ */
    // Start
    /* ------------------------------------------------------------------------------------------ */
    int32 main_capital_index{0};
    FRegistryEntityHandle main_capital_handle;
    FighterSample initial_main_fighters;

    void initial_setup() {
        TestRunner->AddInfo(TEXT("fn: initial_setup"));

        auto& world{spawner->GetWorld()};
        test_driver = ml::TestSimulationDriver::from_world(world);

        capitals = test_driver->orchestrator.get_capital_ships();
        ASSERT_THAT(IsNotNull(capitals));

        fighters = test_driver->orchestrator.get_capital_ship_fighters();
        ASSERT_THAT(IsNotNull(fighters));

        main_capital_index = *capitals->find_first_index_on_team(main_capital_team);
        main_capital_handle = capitals->get_handle(main_capital_index);

        TestRunner->AddInfo(FString::Printf(TEXT("Main capital team: %s"),
                                            *ml::to_string_without_type_prefix(main_capital_team)));

#if WITH_EDITOR
        auto const* settings{GetDefault<USandboxDeveloperSettings>()};
        checks.log_successful_assertions = settings->log_successful_assertions;
#endif
    }
    bool wait_for_fighters_to_spawn() {
        auto const n_fighters{capitals->get_fighters_spawned()};
        if (n_fighters > 0) { return true; }

        return false;
    }

    void initial_sampling_stage() {
        TestRunner->AddInfo(TEXT("fn: initial_sampling_stage"));

        sample_fighter_data(initial_main_fighters);
    }
    void initial_checks_stage() {
        TestRunner->AddInfo(TEXT("fn: initial_checks_stage"));

        check_capitals_not_targeting_self();
        check_main_capital_fighters_not_targeting_parent(initial_main_fighters, TEXT("Initial"));

        log_capital_fighter_info(initial_main_fighters, TEXT("Initial"));
    }
    void initial_setup_stage() {
        TestRunner->AddInfo(TEXT("fn: initial_setup_stage"));

        initial_setup();
        initial_sampling_stage();
        initial_checks_stage();
    }

    /* ------------------------------------------------------------------------------------------ */
    // Killing fighters
    /* ------------------------------------------------------------------------------------------ */
    TArray<FRegistryEntityHandle> destroyed;
    TArray<FRegistryEntityHandle> kept;
    FighterSample post_fighter_kill_main_fighters;

    void run_spawn_capital_handle_checks() {
        TestRunner->AddInfo(TEXT("fn: run_spawn_capital_handle_checks"));

        auto const n_capitals{capitals->get_num_instances()};

        checks.are_equal(n_capitals_exp, n_capitals, TEXT("Number of capitals."));

        auto const capital_fighters_spawn_slots{capitals->get_fighter_spawn_slots()};

        auto const n_fighters_spawned_exp{n_capitals * capital_fighters_spawn_slots};
        auto const n_fighters_spawned{capitals->get_fighters_spawned()};

        checks.are_equal(n_fighters_spawned_exp,
                         n_fighters_spawned,
                         TEXT("Number of fighters (according to capitals)"));

        auto const n_fighters_exp{n_fighters_spawned};
        auto const n_fighters{fighters->get_num_instances()};

        checks.are_equal(n_fighters_exp, n_fighters, TEXT("Number of fighters."));

        auto const& capital_fighter_handles{capitals->get_fighter_handles()};
        auto const& capital_fighter_handle_spans{capitals->get_capital_fighter_handle_spans()};

        auto const n_fighter_handles_exp{n_capitals * capital_fighters_spawn_slots};
        auto const n_fighter_handles{capital_fighter_handles.Num()};
        checks.are_equal(
            n_fighter_handles_exp, n_fighter_handles, TEXT("Number of fighter handles"));

        auto const n_fighter_spans_exp{n_capitals};
        auto const n_fighter_spans{capital_fighter_handle_spans.Num()};
        checks.are_equal(n_fighter_spans_exp, n_fighter_spans, TEXT("Fighter handle spans"));

        for (int32 i{0}; i < n_capitals; ++i) {
            auto const span{capital_fighter_handle_spans[i]};

            auto const span_count_exp{capital_fighters_spawn_slots};
            auto const span_count{span.count};

            checks.are_equal(
                span_count_exp, span_count, FString::Printf(TEXT("Span count (%d)"), i));
        }

        // Check handle uniqueness
        TSet<FRegistryEntityHandle> unique_handles;
        for (int32 i{}; i < n_fighter_handles; ++i) {
            auto const& handle{capital_fighter_handles[i]};

            auto const is_unique{
                checks.is_true(!unique_handles.Contains(handle),
                               FString::Printf(TEXT("[%d] Unique capital fighter handle"), i))};
            if (is_unique) { unique_handles.Add(handle); }
        }

        if (!checks.all_passed) {
            FString msg;
            msg += FString::Printf(TEXT("Failed on tick: %llu"),
                                   test_driver->orchestrator.get_tick_count());

            msg += TEXT("\nCapital ship fighter handles:");
            for (auto const& handle : capital_fighter_handles) {
                msg += FString::Printf(TEXT("\n    %s"), *handle.to_string());
            }

            msg += TEXT("\nCapital ship fighter handles spans:");
            for (int32 span_i{0}; span_i < n_capitals; ++span_i) {
                msg += FString::Printf(TEXT("\n        Capital %d"), span_i);

                auto const& span{capital_fighter_handle_spans[span_i]};

                auto const offset{span.offset};
                auto const count{span.count};
                auto const end{span.end()};

                for (int32 i{offset}; i < end; ++i) {
                    msg += FString::Printf(
                        TEXT("\n    %d: %s"), i, *capital_fighter_handles[i].to_string());
                }
            }

            TestRunner->AddInfo(msg);
        }
    }
    void pre_fighter_kill_stage() {
        TestRunner->AddInfo(TEXT("fn: pre_fighter_kill_stage"));

        run_spawn_capital_handle_checks();
        ASSERT_THAT(IsTrue(checks.all_passed, TEXT("all_passed")));
    }

    void kill_fighters() {
        TestRunner->AddInfo(TEXT("fn: kill_fighters"));

        auto const fighter_handles{capitals->get_fighter_handles()};
        auto const fighter_handle_spans{capitals->get_capital_fighter_handle_spans()};

        destroyed.Reset();
        kept.Reset();

        for (auto const span : fighter_handle_spans) {
            auto const end{span.end()};
            for (int32 i{span.offset}; i < end; ++i) {
                auto const handle{fighter_handles[i]};
                if (i % 2 == 0) {
                    test_driver->queue_damage(handle, 100000, {});
                    destroyed.Add(handle);
                } else {
                    kept.Add(handle);
                }
            }
        }
    }
    void kill_fighters_stage() {
        kill_fighters();
        test_driver->set_wait_until_tick_from_now(cycles_to_wait);
    }

    void post_fighter_kill_sample_stage() {
        TestRunner->AddInfo(TEXT("fn: post_fighter_kill_sample_stage"));

        sample_fighter_data(post_fighter_kill_main_fighters);
    }
    void check_fighter_handles_after_fighter_kills() {
        TestRunner->AddInfo(TEXT("fn: check_handles_after_fighter_kills"));

        auto const fighter_handles{capitals->get_fighter_handles()};
        auto const fighter_handle_spans{capitals->get_capital_fighter_handle_spans()};

        int32 total_left{0};
        for (auto const span : fighter_handle_spans) {
            auto const count{span.count};
            total_left += count;

            auto const end{span.end()};
            for (int32 i{span.offset}; i < end; ++i) {
                auto const handle{fighter_handles[i]};
                checks.is_true(kept.Contains(handle),
                               FString::Printf(TEXT("[%d] Fighter handle is kept"), i));
                checks.is_true(!destroyed.Contains(handle),
                               FString::Printf(TEXT("[%d] Fighter handle not destroyed"), i));
            }
        }

        // Look directly at kept/destroyed
        for (int32 i{0}; i < kept.Num(); ++i) {
            checks.is_true(
                test_driver->registry.is_valid_alive(kept[i]), TEXT("Kept is valid alive"), i);
        }
        for (int32 i{0}; i < kept.Num(); ++i) {
            checks.is_true(test_driver->registry.is_valid_dead(destroyed[i]),
                           TEXT("Destroyed is valid dead"),
                           i);
        }

        checks.are_equal(kept.Num(), total_left, TEXT("Expected left"));
    }
    void check_remaining_main_capital_fighters_unchanged() {
        auto const n_post{post_fighter_kill_main_fighters.handles.Num()};

        for (int32 i{}; i < n_post; ++i) {
            auto const handle{post_fighter_kill_main_fighters.handles[i]};
            checks.is_true(
                initial_main_fighters.handles.Contains(handle),
                FString::Printf(
                    TEXT("Post-kill main fighter[%d] (%s) was parented by main before kill"),
                    i,
                    *handle.to_string()));
        }
    }
    void post_fighter_kill_check_stage() {
        TestRunner->AddInfo(TEXT("fn: post_fighter_kill_check_stage"));

        check_fighter_handles_after_fighter_kills();
        check_main_capital_fighters_not_targeting_parent(post_fighter_kill_main_fighters,
                                                         TEXT("Post-kill fighters"));

        check_remaining_main_capital_fighters_unchanged();
    }
    void post_fighter_kill_stage() {
        TestRunner->AddInfo(TEXT("fn: post_fighter_kill_stage"));

        post_fighter_kill_sample_stage();
        log_capital_fighter_info(post_fighter_kill_main_fighters, TEXT("Post fighter kill"));
        post_fighter_kill_check_stage();

        ASSERT_THAT(IsTrue(checks.all_passed, TEXT("all_passed")));
    }

    /* ------------------------------------------------------------------------------------------ */
    // Kill a capital
    /* ------------------------------------------------------------------------------------------ */
    int32 n_fighters_before_capital_kill{0};
    int32 n_main_capital_fighters_before_capital_kill{0};

    FighterSample pre_capital_kill_main_fighters;
    FighterSample post_capital_kill_main_fighters;

    void save_data_before_capital_kill() {
        TestRunner->AddInfo(TEXT("fn: save_fighter_info_before_capital_kill"));

        n_fighters_before_capital_kill = fighters->get_num_instances();

        auto const main_span{capitals->get_capital_fighter_handle_span(main_capital_index)};
        n_main_capital_fighters_before_capital_kill = main_span.count;

        sample_fighter_data(pre_capital_kill_main_fighters);
    }

    void check_main_fighters_after_event(
        FighterSample const& before, FighterSample const& after, FString const info) {}

    void check_main_capital_fighters_not_targeting_parent(FighterSample const& sample,
                                                          FString const info) {
        TestRunner->AddInfo(TEXT("fn: check_main_capital_fighters_not_targeting_parent"));

        auto const n{sample.target_handles.Num()};

        for (int32 i{0}; i < n; ++i) {
            checks.not_equal(main_capital_handle,
                             sample.target_handles[i],
                             FString::Printf(TEXT("(%s) Fighter (%s) not targeting parent"),
                                             *info,
                                             *sample.handles[i].to_string()),
                             i);
        }
    }
    void check_capital_state_after_kill() {
        TestRunner->AddInfo(TEXT("fn: check_capital_state_after_kill"));

        auto const n_capitals{capitals->get_num_instances()};
        checks.are_equal(n_capitals, n_capitals_exp - 1, TEXT("Check capital count after kill"));

        // Check fighter ownership
        auto const fighter_spans{capitals->get_capital_fighter_handle_spans()};
        auto const fighter_handles{capitals->get_fighter_handles()};

        // save original handles in a sorted array
        // save new handles in a sorted array
        // compare each element
    }
    void check_fighter_tasks_after_kill() {
        TestRunner->AddInfo(TEXT("fn: check_fighter_tasks_after_kill"));

        auto const fighter_tasks{fighters->get_tasks()};
        auto const n{fighter_tasks.Num()};

        for (int32 i{}; i < n; ++i) {
            auto const task{fighter_tasks[i]};
            checks.are_equal(task,
                             ETestCapitalShipFightersTask::Attack,
                             FString::Printf(TEXT("Check is attacking: %d"), i));
        }
    }
    void check_num_fighters_after_kill() {
        TestRunner->AddInfo(TEXT("fn: check_num_fighters_after_kill"));

        auto const n_fighters{fighters->get_num_instances()};

        checks.are_equal(n_fighters_before_capital_kill,
                         n_fighters,
                         TEXT("Check number of fighters is unchanged."));
    }
    void check_capital_fighter_handles_unchanged(FighterSample const& before,
                                                 FighterSample const& after) {
        TestRunner->AddInfo(TEXT("fn: check_capital_fighter_handles_unchanged"));

        auto const n_original{before.handles.Num()};

        if (!checks.are_equal(n_original,
                              after.handles.Num(),
                              TEXT("Check num main capital fighter handles unchanged"))) {
            return;
        }

        checks.all_equal(
            after.handles, after.handles, TEXT("Check main cap fighter handle is unchanged"));
    }
    void check_fighters_target_location_changed() {
        TestRunner->AddInfo(TEXT("fn: check_fighters_target_location_changed"));

        auto const n_original_locations{pre_capital_kill_main_fighters.target_locations.Num()};
        auto const n_new_locations{post_capital_kill_main_fighters.target_locations.Num()};

        auto const n_original_handles{pre_capital_kill_main_fighters.target_handles.Num()};
        auto const n_new_handles{post_capital_kill_main_fighters.target_handles.Num()};

        auto const n_exp{n_main_capital_fighters_before_capital_kill};
        checks.are_equal(n_exp,
                         n_original_locations,
                         FString::Printf(TEXT("Correct number of original locations")));
        checks.are_equal(
            n_exp, n_new_locations, FString::Printf(TEXT("Correct number of new locations")));
        checks.are_equal(
            n_exp, n_original_handles, FString::Printf(TEXT("Correct number of original handles")));
        checks.are_equal(
            n_exp, n_new_handles, FString::Printf(TEXT("Correct number of new handles")));

        if (!checks.all_passed) { return; }

        for (int32 i{0}; i < n_new_locations; ++i) {
            auto const old_loc{pre_capital_kill_main_fighters.target_locations[i]};
            auto const new_loc{post_capital_kill_main_fighters.target_locations[i]};

            auto const old_handle{pre_capital_kill_main_fighters.target_handles[i]};
            auto const new_handle{post_capital_kill_main_fighters.target_handles[i]};

            auto const locs_equal{old_loc == new_loc};
            auto const handles_equal{old_loc == new_loc};

            checks.not_equal(
                old_loc, new_loc, FString::Printf(TEXT("Check locations not the same [%d]."), i));

            checks.not_equal(old_handle,
                             new_handle,
                             FString::Printf(TEXT("Check handles not the same [%d]"), i));
        }
    }

    void kill_capital_opponent() {
        TestRunner->AddInfo(TEXT("fn: kill_capital_opponent"));

        auto const target{capitals->get_target_handle(main_capital_index)};
        auto const target_team{capitals->get_team(target)};

        TestRunner->AddInfo(FString::Printf(TEXT("Killing handle %s (team: %s)"),
                                            *target.to_string(),
                                            *ml::to_string_without_type_prefix(target_team)));

        checks.is_true(test_driver->registry.is_valid_alive(target),
                       TEXT("Check target alive before kill"));
        test_driver->queue_kill(target, {});
    }
    void save_data_after_capital_kill() {
        TestRunner->AddInfo(TEXT("fn: save_data_after_capital_kill"));

        sample_fighter_data(post_capital_kill_main_fighters);
    }

    void pre_capital_kill_stage() {
        TestRunner->AddInfo(TEXT("fn: pre_capital_kill_stage"));

        save_data_before_capital_kill();

        check_main_capital_fighters_not_targeting_parent(pre_capital_kill_main_fighters,
                                                         TEXT("Pre-kill capital"));
    }
    void capital_kill_stage() {
        TestRunner->AddInfo(TEXT("fn: capital_kill_stage"));

        kill_capital_opponent();
        test_driver->set_wait_until_tick_from_now(30);
    }
    void post_capital_kill_stage() {
        TestRunner->AddInfo(TEXT("fn: post_capital_kill_stage"));

        save_data_after_capital_kill();

        check_main_capital_fighters_not_targeting_parent(post_capital_kill_main_fighters,
                                                         TEXT("Post-kill capital"));
        check_num_fighters_after_kill();
        check_capitals_not_targeting_self();
        check_capital_state_after_kill();
        check_fighter_tasks_after_kill();
        check_capital_fighter_handles_unchanged(pre_capital_kill_main_fighters,
                                                post_capital_kill_main_fighters);
        check_fighters_target_location_changed();
        ASSERT_THAT(IsTrue(checks.all_passed, TEXT("all_passed")));
    }

    inline static FTimespan const default_timeout{0, 0, 3};

    TEST_METHOD(KillFightersOnly)
    {
        TestCommandBuilder.StartWhen([this] { return nullptr != spawner->FindFirstPlayerPawn(); })
            .Then([this] { initial_setup_stage(); })
            .Until([this]() -> bool { return wait_for_fighters_to_spawn(); }, default_timeout)
            // Fighter kill
            .Then([this] { pre_fighter_kill_stage(); })
            .Then([this] { kill_fighters_stage(); })
            .Until([this] { return test_driver->wait_is_over(); }, default_timeout)
            .Then([this] { post_fighter_kill_stage(); });
    }
    TEST_METHOD(KillCapital)
    {

        TestCommandBuilder.StartWhen([this] { return nullptr != spawner->FindFirstPlayerPawn(); })
            .Then([this] { initial_setup_stage(); })
            // Capital kill
            .Then([this] { pre_capital_kill_stage(); })
            .Then([this] { capital_kill_stage(); })
            .Until([this] { return test_driver->wait_is_over(); }, default_timeout)
            .Then([this] { post_capital_kill_stage(); });
    }
    TEST_METHOD(All)
    {
        TestCommandBuilder.StartWhen([this] { return nullptr != spawner->FindFirstPlayerPawn(); })
            .Then([this] { initial_setup_stage(); })
            .Until([this]() -> bool { return wait_for_fighters_to_spawn(); }, default_timeout)
            .Then([this] { pre_fighter_kill_stage(); })
            // Fighter kill
            .Then([this] { kill_fighters_stage(); })
            .Until([this] { return test_driver->wait_is_over(); }, default_timeout)
            .Then([this] { post_fighter_kill_stage(); })
            // Capital kill
            .Then([this] { pre_capital_kill_stage(); })
            .Then([this] { capital_kill_stage(); })
            .Until([this] { return test_driver->wait_is_over(); }, default_timeout)
            .Then([this] { post_capital_kill_stage(); });
    }
};
