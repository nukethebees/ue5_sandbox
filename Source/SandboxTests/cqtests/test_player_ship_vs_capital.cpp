#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestSpaceShip.h>

#include <SandboxTests/cqtests/level_checks.h>
#include <SandboxTests/cqtests/SoftTestAssertions.h>
#include <SandboxTests/cqtests/test_setup.h>
#include <SandboxTests/cqtests/TestSimulationDriver.h>

#include <SandboxCore/time_series_data.h>

#include <Components/MapTestSpawner.h>
#include <CQTest.h>
#include <Misc/Optional.h>

TEST_CLASS(PlayerShipVsCapital, "Sandbox.FunctionalTests")
{
    using ThisClass = PlayerShipVsCapital;

    using time_type = ml::TestSimulationDriver::time_type;

    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    ml::FSoftTestAssertions checks{};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};
    ATestSpaceShip const* player_ship{nullptr};
    ATestCapitalShips const* capitals{nullptr};
    ATestCapitalShipFighters const* fighters{nullptr};

    FRegistryEntityHandle player_ship_handle{};

    ml::TimeSeriesData<FVector> player_ship_locations;
    ml::TimeSeriesData<FVector> player_ship_registry_locations;
    ml::TimeSeriesData<TArray<FVector3f>> fighter_target_locations;
    ml::TimeSeriesData<TArray<FVector3f>> fighter_locations;

    BEFORE_EACH()
    { spawner = ml::level_test_setup(TEXT("FuncT_player_ship_vs_capital"), TestRunner, checks); }
    AFTER_EACH()
    { test_driver->orchestrator.clear_end_tick_test_hook(); }
  private:
    void sample_values(ATestBatchOrchestrator & orchestrator) {
        auto const t{test_driver->get_time()};

        player_ship_locations.add(t, player_ship->GetActorLocation());
        player_ship_registry_locations.add(
            t, FVector{test_driver->registry.get_location(player_ship_handle)});

        fighter_target_locations.add(t, ml::to_vector3f_array(fighters->get_target_locations()));
        fighter_locations.add(t, ml::to_vector3f_array(fighters->get_locations()));
    }

    /* ---------------------------------------------------------------------------- */
    // Initial phase
    /* ---------------------------------------------------------------------------- */
    static constexpr time_type initial_wait{0.1};
    static constexpr time_type track_time{0.5};

    static constexpr time_type t_start{0.0};
    static constexpr time_type t_settled{t_start + initial_wait};
    static constexpr time_type t_tracked{t_settled + track_time};
    static constexpr time_type t_end{t_tracked + time_type{5.0}};

    FTimespan timeout{0, 0, FMath::CeilToInt32(t_end + 1.0)};

    void initial_setup() {
        auto& world{spawner->GetWorld()};
        test_driver = ml::TestSimulationDriver::from_world(world);

        player_ship = &test_driver->get_player_ship();
        capitals = &test_driver->get_capital_ships();
        fighters = &test_driver->get_capital_ship_fighters();
        player_ship_handle = player_ship->get_entity_handle();
        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::sample_values));
    }

    void full_checks() {
        int32 const i_setup{fighter_target_locations.nearest_index(t_settled)};
        int32 const i_tracked{fighter_target_locations.nearest_index(t_tracked)};
        int32 const i_end{fighter_target_locations.nearest_index(t_end)};

        constexpr time_type before_end{0.5};
        int32 const i_before_end{fighter_target_locations.nearest_index(t_end - before_end)};

        checks.dist_zero(player_ship_locations.value_at(i_setup),
                         player_ship_registry_locations.value_at(i_setup),
                         1.0,
                         TEXT("Registry and ship locations same at start."));

        checks.dist_zero(player_ship_locations.value_at(i_tracked),
                         player_ship_registry_locations.value_at(i_tracked),
                         1.0,
                         TEXT("Registry and ship locations same after some time."));

        checks.not_dist_zero(player_ship_locations.value_at(i_setup),
                             player_ship_locations.value_at(i_tracked),
                             1.0,
                             TEXT("Ship moves"));

        checks.is_greater_than(ml::num(fighter_target_locations.value_at(i_tracked)),
                               int32{0},
                               FString{TEXT("Non-zero target locations")});

        auto const n_locs{fighter_target_locations.value_at(i_tracked).Num()};
        checks.are_equal(n_locs,
                         fighter_target_locations.value_at(i_end).Num(),
                         TEXT("Check num locations equal"));

        checks.are_equal(n_locs,
                         fighter_locations.value_at(i_end).Num(),
                         TEXT("Check num fighter locations same as target locations"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        for (int32 i{0}; i < n_locs; ++i) {
            checks.not_dist_zero(fighter_target_locations.value_at(i_tracked)[i],
                                 fighter_target_locations.value_at(i_end)[i],
                                 1.0,
                                 TEXT("Check fighter target location updates"),
                                 i);
        }

        constexpr double expected_min_distance_moved{500.0};
        for (int32 i{0}; i < n_locs; ++i) {
            checks.dist_greater_than(fighter_locations.value_at(i_before_end)[i],
                                     fighter_locations.value_at(i_end)[i],
                                     expected_min_distance_moved,
                                     TEXT("Check fighter moves late in sim"),
                                     i);

            checks.dist_greater_than(fighter_target_locations.value_at(i_before_end)[i],
                                     fighter_target_locations.value_at(i_end)[i],
                                     expected_min_distance_moved,
                                     TEXT("Check fighter target location updates late in sim"),
                                     i);
        }

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }

    TEST_METHOD(Main)
    {

        TestCommandBuilder
            // Initial phase
            .Do([this] {
                initial_setup();
                test_driver->set_delta_time_wait(t_end);
            })
            .Until([this] { return test_driver->time_wait_completed(); }, timeout)
            .Do([this] { full_checks(); });
    }
};
