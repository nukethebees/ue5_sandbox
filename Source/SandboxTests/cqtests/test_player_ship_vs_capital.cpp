#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestSpaceShip.h>

#include <SandboxTests/cqtests/level_checks.h>
#include <SandboxTests/cqtests/SoftTestAssertions.h>
#include <SandboxTests/cqtests/test_setup.h>
#include <SandboxTests/cqtests/TestSimulationDriver.h>

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

    BEFORE_EACH()
    { spawner = ml::level_test_setup(TEXT("FuncT_player_ship_vs_capital"), TestRunner, checks); }
  private:
    /* ---------------------------------------------------------------------------- */
    // Initial phase
    /* ---------------------------------------------------------------------------- */
    static constexpr time_type initial_wait{0.5};

    FVector player_ship_initial_location;
    FVector player_ship_initial_registry_location;

    void initial_setup() {
        auto& world{spawner->GetWorld()};
        test_driver = ml::TestSimulationDriver::from_world(world);

        player_ship = &test_driver->get_player_ship();
        capitals = &test_driver->get_capital_ships();
        fighters = &test_driver->get_capital_ship_fighters();
    }
    void initial_samples() {
        player_ship_handle = player_ship->get_entity_handle();

        player_ship_initial_location = player_ship->GetActorLocation();
        player_ship_initial_registry_location =
            FVector{test_driver->registry.get_location(player_ship_handle)};
    }
    void initial_checks() {
        checks.dist_zero(player_ship_initial_location,
                         player_ship_initial_registry_location,
                         1.0,
                         TEXT("Registry and ship locations same at start."));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }
    void initial_phase() {
        initial_setup();
        initial_samples();
        initial_checks();

        test_driver->set_delta_time_wait(initial_wait);
    }

    /* ---------------------------------------------------------------------------- */
    // Post settling phase
    /* ---------------------------------------------------------------------------- */
    FVector player_ship_next_location;
    FVector player_ship_next_registry_location;

    void post_settling_samples() {
        player_ship_next_location = player_ship->GetActorLocation();
        player_ship_next_registry_location =
            FVector{test_driver->registry.get_location(player_ship_handle)};
    }
    void post_settling_checks() {
        checks.dist_zero(player_ship_next_location,
                         player_ship_next_registry_location,
                         1.0,
                         TEXT("Registry and ship locations same after some time."));

        checks.not_dist_zero(
            player_ship_initial_location, player_ship_next_location, 1.0, TEXT("Ship moves"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }
    void post_settling_phase() {
        post_settling_samples();
        post_settling_checks();

        test_driver->set_delta_time_wait(initial_wait);
    }

    TEST_METHOD(Main)
    {
        auto check_wait_over{[this] { return test_driver->time_wait_completed(); }};

        TestCommandBuilder
            // Initial phase
            .Do([this] { initial_phase(); })
            .Until(check_wait_over, default_timeout)
            .Do([this] { post_settling_phase(); });
    }
};
