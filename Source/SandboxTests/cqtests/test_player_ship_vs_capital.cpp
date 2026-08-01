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
    using time_type = ml::TestSimulationDriver::time_type;

    inline static FTimespan const default_timeout{0, 0, 1};

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

    BEFORE_EACH()
    { spawner = ml::level_test_setup(TEXT("FuncT_player_ship_vs_capital"), TestRunner, checks); }
  private:
    void sample_values() {
        auto const t{test_driver->get_time()};

        player_ship_locations.add(t, player_ship->GetActorLocation());
        player_ship_registry_locations.add(
            t, FVector{test_driver->registry.get_location(player_ship_handle)});

        fighter_target_locations.add(t, ml::to_vector3f_array(fighters->get_target_locations()));
    }

    /* ---------------------------------------------------------------------------- */
    // Initial phase
    /* ---------------------------------------------------------------------------- */
    static constexpr time_type initial_wait{0.1};

    void initial_setup() {
        auto& world{spawner->GetWorld()};
        test_driver = ml::TestSimulationDriver::from_world(world);

        player_ship = &test_driver->get_player_ship();
        capitals = &test_driver->get_capital_ships();
        fighters = &test_driver->get_capital_ship_fighters();
        player_ship_handle = player_ship->get_entity_handle();
    }
    void initial_checks() {
        int32 const cur{fighter_target_locations.last_index()};

        checks.dist_zero(player_ship_locations.last_value(),
                         player_ship_registry_locations.last_value(),
                         1.0,
                         TEXT("Registry and ship locations same at start."));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }
    void initial_phase() {
        initial_setup();
        sample_values();
        initial_checks();

        test_driver->set_delta_time_wait(initial_wait);
    }

    /* ---------------------------------------------------------------------------- */
    // Post settling phase
    /* ---------------------------------------------------------------------------- */
    void post_settling_checks() {
        int32 const cur{fighter_target_locations.last_index()};
        int32 const prev{cur - 1};

        checks.dist_zero(player_ship_locations.value_at(cur),
                         player_ship_registry_locations.value_at(cur),
                         1.0,
                         TEXT("Registry and ship locations same after some time."));

        checks.not_dist_zero(player_ship_locations.value_at(prev),
                             player_ship_locations.value_at(cur),
                             1.0,
                             TEXT("Ship moves"));

        checks.is_greater_than(ml::num(fighter_target_locations.value_at(cur)),
                               int32{0},
                               FString{TEXT("Non-zero target locations")});

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }
    void post_settling_phase() {
        sample_values();
        post_settling_checks();
    }

    /* ---------------------------------------------------------------------------- */
    // Let fighters track
    /* ---------------------------------------------------------------------------- */
    static constexpr time_type track_time{0.5};

    void fighter_track_checks() {
        int32 const cur{fighter_target_locations.last_index()};
        int32 const prev{cur - 1};

        auto const n_locs{fighter_target_locations.value_at(prev).Num()};
        checks.are_equal(n_locs,
                         fighter_target_locations.value_at(cur).Num(),
                         TEXT("Check num locations equal"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        for (int32 i{0}; i < n_locs; ++i) {
            checks.not_dist_zero(fighter_target_locations.value_at(prev)[i],
                                 fighter_target_locations.value_at(cur)[i],
                                 1.0,
                                 TEXT("Check fighter target location updates"),
                                 i);
        }

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }
    void fighter_track_phase() {
        sample_values();
        fighter_track_checks();
    }

    TEST_METHOD(Main)
    {
        auto check_wait_over{[this] { return test_driver->time_wait_completed(); }};

        TestCommandBuilder
            // Initial phase
            .Do([this] { initial_phase(); })
            .Until(check_wait_over, default_timeout)
            .Do([this] { post_settling_phase(); })
            .Do([this] { test_driver->set_delta_time_wait(track_time); })
            .Until(check_wait_over, default_timeout)
            .Do([this] { fighter_track_phase(); });
    }
};
