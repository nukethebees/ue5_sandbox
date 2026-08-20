#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/TestActorSpawning.h>

#include <Sandbox/batch_game/SimulationConfig.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>
#include <Sandbox/batch_game/TestTeam.h>
#include <SandboxGameShared/core/SandboxDeveloperSettings.h>
#include <SandboxGameShared/utilities/enums.h>

#include <SandboxTests/SandboxTestLogCategories.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestSimulationDriver.h>
#include <SandboxTests/support/time_series_test_data.h>

#include <SandboxCore/array_math.h>
#include <SandboxCore/container_ops.h>
#include <SandboxCore/time_series_data.h>

#include <SandboxCoreEngine/actor_utils.h>

#include <Components/MapTestSpawner.h>
#include <Containers/Map.h>
#include <CQTest.h>
#include <Editor.h>
#include <Engine/World.h>
#include <EngineUtils.h>
#include <Misc/Optional.h>

TEST_CLASS(TestEntityRegistry, "Sandbox.LevelTests")
{
    using ThisClass = TestEntityRegistry;
    using time_type = ml::TestSimulationDriver::time_type;

    /* ------------------------------------------------------------------------------------------ */
    // Shared test setup
    /* ------------------------------------------------------------------------------------------ */
    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};
    ml::FSoftTestAssertions checks{};

    /* ------------------------------------------------------------------------------------------ */
    // Team-count test
    /* ------------------------------------------------------------------------------------------ */
    struct FTeamCountTestState {
        ml::TimeSeriesData<FTestEntityRegistry::TeamCounts> alive_per_team;
        ml::TimeSeriesData<FTestEntityRegistry::EntityCounts> alive_per_team_and_type;
        TMap<ETestTeam, int32> expected_teams{
            {ETestTeam::White, 0},
            {ETestTeam::Red, 1},
            {ETestTeam::Green, 2},
            {ETestTeam::Blue, 3},
            {ETestTeam::Orange, 4},
            {ETestTeam::Yellow, 5},
        };
    };
    TUniquePtr<FTeamCountTestState> team_count_state{nullptr};

    /* ------------------------------------------------------------------------------------------ */
    // Variable player-kill test
    /* ------------------------------------------------------------------------------------------ */
    struct FVariableKillSample {
        int32 player_kills{0};
        int32 total_kills{0};
        int32 alive_count{0};
    };
    struct FVariableKillTestState {
        ml::TimeSeriesData<FVariableKillSample> samples;
        TestEntityUniqueId player_id{};
        int32 expected_kills{0};
        int32 initial_alive_count{0};
    };
    TUniquePtr<FVariableKillTestState> variable_kill_state{nullptr};
    FDelegateHandle player_spawn_map_change_handle{};
    bool test_ready{false};

    BEFORE_EACH()
    {
        test_driver.Reset();
        ml::reset(spawner, team_count_state, variable_kill_state);
        test_ready = false;
    }
    AFTER_EACH()
    {
        if (player_spawn_map_change_handle.IsValid()) {
            FEditorDelegates::MapChange.Remove(player_spawn_map_change_handle);
            player_spawn_map_change_handle.Reset();
        }
        if (test_driver.IsSet()) {
            test_driver->orchestrator.clear_end_tick_test_hook();
            test_driver->orchestrator.pause_simulation();
        }

        ml::reset(test_driver, spawner, team_count_state, variable_kill_state);
    }
  private:
    /* ------------------------------------------------------------------------------------------ */
    // Team-count test
    /* ------------------------------------------------------------------------------------------ */
    void setup_team_count_level() {
        spawner = ml::level_test_setup(TEXT("FuncT_entity_registry"), TestRunner, checks);
    }

    /* ------------------------------------------------------------------------------------------ */
    // Variable player-kill test
    /* ------------------------------------------------------------------------------------------ */
    void setup_variable_kill_level() {
        player_spawn_map_change_handle =
            FEditorDelegates::MapChange.AddLambda([this](uint32 const flags) {
                if (test_ready || !(flags & MapChangeEventFlags::NewMap)) {
                    return;
                }

                auto editor_world{ml::get_editor_world()};
                if (!editor_world) {
                    UE_LOG(LogSandboxTest, Error, TEXT("%s"), *editor_world.error());
                    checks.is_true(false, TEXT("Editor world is available"));
                    return;
                }
                auto* const world{*editor_world};

                auto const* const test_config{ml::get_default_test_config(checks)};
                auto const* const simulation_config{ml::get_default_simulation_config(checks)};
                if (!test_config || !simulation_config) {
                    return;
                }

                auto* const player_config{simulation_config->player_ship_config.Get()};
                auto* const player_ship{ml::spawn_player_ship(
                    *world, test_config->actor_classes.player_ship_class, player_config)};
                if (!checks.is_valid(player_ship, TEXT("Player ship is spawned"))) {
                    return;
                }

                auto* const orchestrator{ml::get_first_actor<ATestBatchOrchestrator>(*world)};
                if (!checks.is_valid(orchestrator, TEXT("Orchestrator is available"))) {
                    return;
                }

                orchestrator->set_player_ship(*player_ship);
                test_ready = true;
            });

        spawner = ml::level_test_setup(TEXT("FuncT_entity_registry"), TestRunner, checks);
    }

    /* ------------------------------------------------------------------------------------------ */
    // Team-count test
    /* ------------------------------------------------------------------------------------------ */
    static constexpr time_type test_time{0.1};

    void initial_setup() {
        team_count_state = MakeUnique<FTeamCountTestState>();
        auto& world{spawner->GetWorld()};
        test_driver = ml::TestSimulationDriver::from_world(world);
        test_driver->orchestrator.start_simulation();
    }

    auto get_total_expected() const -> int32 {
        check(team_count_state);
        int32 total{0};

        for (auto const& [_, v] : team_count_state->expected_teams) {
            total += v;
        }

        return total;
    }
    void sample_values(ATestBatchOrchestrator&) {
        check(team_count_state);
        team_count_state->alive_per_team.add(test_driver->get_time(),
                                             test_driver->registry.count_alive_per_team());
        team_count_state->alive_per_team_and_type.add(
            test_driver->get_time(), test_driver->registry.count_alive_per_team_and_type());
    }
    void on_end_tick(ATestBatchOrchestrator & orchestrator) {
        sample_values(orchestrator);
        test_driver->timeline.tick(test_driver->get_time());
    }

    void run_checks() {
        check(team_count_state);
        count_teams(team_count_state->alive_per_team.nearest_index(test_time));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }
    auto get_expected_team_counts_msg() const -> FString {
        check(team_count_state);
        FString msg{TEXT("Expected team counts:")};

        for (auto const& [k, v] : team_count_state->expected_teams) {
            msg += FString::Printf(TEXT("\n    %s: %d"), *ml::to_string_without_type_prefix(k), v);
        }

        return msg;
    }
    auto get_team_counts(TConstArrayView<int32> const counts) const -> FString {
        FString msg;

        auto const n{ml::EnumCountTrait<ETestEntityType>::count_value};
        for (int32 i{}; i < n; ++i) {
            msg +=
                FString::Printf(TEXT("\n%s: %d"),
                                *ml::to_string_without_type_prefix(static_cast<ETestEntityType>(i)),
                                counts[i]);
        }

        return msg;
    }
    void count_teams(int32 const sample_index) {
        check(team_count_state);
        TMap<ETestTeam, int32> teams_counted;

        auto const& counts{team_count_state->alive_per_team.value_at(sample_index)};
        auto const counts_view{TConstArrayView<int32>(counts)};
        auto const count_sum{ml::sum(counts_view)};

        checks.are_equal(get_total_expected(), count_sum, [&] -> FString {
            FString msg{TEXT("Check entity total.")};
            msg += get_team_counts(counts_view);
            return msg;
        }());

        auto const n_teams{ml::EnumCountTrait<ETestTeam>::count_value};
        for (int32 i{}; i < n_teams; ++i) {
            teams_counted.Emplace(static_cast<ETestTeam>(i), counts[i]);
        }

        for (auto const& [team, exp_count] : team_count_state->expected_teams) {
            auto const count{counts[std::to_underlying(team)]};
            checks.are_equal(exp_count, count, [&] -> FString {
                return FString::Printf(TEXT("Count team %s"),
                                       *ml::to_string_without_type_prefix(team));
            }());

            auto const& type_counts{
                team_count_state->alive_per_team_and_type.value_at(sample_index)};
            int32 type_count{0};
            constexpr auto n_types{ml::EnumCountTrait<ETestEntityType>::count_value};
            for (int32 type{0}; type < n_types; ++type) {
                type_count += type_counts[std::to_underlying(team)][type];
            }
            checks.are_equal(count, type_count, [&] -> FString {
                return FString::Printf(TEXT("Count team/type matrix for %s"),
                                       *ml::to_string_without_type_prefix(team));
            }());
        }
    }

    /* ------------------------------------------------------------------------------------------ */
    // Variable player-kill test
    /* ------------------------------------------------------------------------------------------ */
    static constexpr time_type before_kill_time{0.05};
    static constexpr time_type kill_time{0.1};
    static constexpr time_type after_kill_time{0.3};
    static constexpr time_type variable_kill_test_duration{0.35};

    void begin_variable_kill_scenario(int32 const expected_kills) {
        variable_kill_state = MakeUnique<FVariableKillTestState>();
        auto& state{*variable_kill_state};
        state.expected_kills = expected_kills;

        test_driver = ml::TestSimulationDriver::from_world(spawner->GetWorld());

        auto const& player_ship{test_driver->get_player_ship()};
        state.player_id = player_ship.get_unique_id();
        state.initial_alive_count = test_driver->registry.count_alive();

        auto const available_targets{
            test_driver->registry.get_handles_not_in_team(player_ship.get_team())};
        if (!checks.is_greater_than(available_targets.Num(),
                                    state.expected_kills - 1,
                                    TEXT("Enough non-player-team targets are available"))) {
            return;
        }

        TArray<FRegistryEntityHandle> targets;
        targets.Append(available_targets.GetData(), state.expected_kills);

        ml::reset_and_reserve_time_series(
            test_driver->orchestrator, variable_kill_test_duration, state.samples);
        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_variable_kill_end_tick));
        test_driver->timeline.then_after(
            kill_time, [this, targets, player_handle = player_ship.get_entity_handle()] {
                test_driver->queue_kills(targets, player_handle);
            });
        test_driver->timeline.finish_at(variable_kill_test_duration);
        test_driver->orchestrator.start_simulation();
    }
    void on_variable_kill_end_tick(ATestBatchOrchestrator&) {
        check(variable_kill_state);

        auto const& registry{test_driver->registry};
        variable_kill_state->samples.add(test_driver->get_time(),
                                         FVariableKillSample{static_cast<int32>(registry.get_kills(
                                                                 variable_kill_state->player_id)),
                                                             registry.count_kills(),
                                                             registry.count_alive()});
        test_driver->timeline.tick(test_driver->get_time());
    }
    void check_variable_kill_results() {
        check(variable_kill_state);
        auto const& state{*variable_kill_state};

        checks.is_greater_than(state.samples.num(), int32{0}, TEXT("Kill samples recorded"));
        if (state.samples.is_empty()) {
            SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
            return;
        }

        auto const& before_kills{state.samples.nearest_value(before_kill_time)};
        auto const& after_kills{state.samples.nearest_value(after_kill_time)};
        auto const& final_kills{state.samples.nearest_value(variable_kill_test_duration)};

        checks.are_equal(0, before_kills.player_kills, TEXT("Player kills are zero before event"));
        checks.are_equal(0, before_kills.total_kills, TEXT("Total kills are zero before event"));
        checks.are_equal(state.initial_alive_count,
                         before_kills.alive_count,
                         TEXT("All entities are alive before event"));

        checks.are_equal(state.expected_kills,
                         after_kills.player_kills,
                         TEXT("Kills are attributed to player ship"));
        checks.are_equal(state.expected_kills,
                         after_kills.total_kills,
                         TEXT("Total kill count matches killed entities"));
        checks.are_equal(state.initial_alive_count - state.expected_kills,
                         after_kills.alive_count,
                         TEXT("Alive count reflects killed entities"));

        checks.are_equal(state.expected_kills,
                         final_kills.player_kills,
                         TEXT("Player kill count remains correct after event"));
        checks.are_equal(state.expected_kills,
                         final_kills.total_kills,
                         TEXT("Total kill count remains correct after event"));
        checks.are_equal(state.initial_alive_count - state.expected_kills,
                         final_kills.alive_count,
                         TEXT("Alive count remains correct after event"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }
    void run_variable_kill_scenario(int32 const expected_kills) {
        setup_variable_kill_level();
        TestCommandBuilder.StartWhen([this] { return test_ready; })
            .Then([this, expected_kills] { begin_variable_kill_scenario(expected_kills); })
            .Until([this] { return test_driver->timeline.is_finished(); }, FTimespan{0, 0, 1})
            .Then([this] { check_variable_kill_results(); });
    }

    /* ------------------------------------------------------------------------------------------ */
    // Team-count test
    /* ------------------------------------------------------------------------------------------ */
    TEST_METHOD(MainTest)
    {
        setup_team_count_level();
        TestCommandBuilder
            .Do([this] {
                initial_setup();
                ml::reset_and_reserve_time_series(test_driver->orchestrator,
                                                  test_time,
                                                  team_count_state->alive_per_team,
                                                  team_count_state->alive_per_team_and_type);
                test_driver->orchestrator.set_end_tick_test_hook(
                    FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));
                test_driver->timeline.finish_at(test_time);
            })
            .Until([this] { return test_driver->timeline.is_finished(); }, FTimespan{0, 0, 1})
            .Then([this] { run_checks(); });
    }

    /* ------------------------------------------------------------------------------------------ */
    // Variable player-kill test
    /* ------------------------------------------------------------------------------------------ */
    TEST_METHOD(OnePlayerKill)
    { run_variable_kill_scenario(1); }

    TEST_METHOD(TwoPlayerKills)
    { run_variable_kill_scenario(2); }
};
