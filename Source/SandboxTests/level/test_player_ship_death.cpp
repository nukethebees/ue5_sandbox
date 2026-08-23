#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestSimulationDriver.h>

#include <SandboxTests/support/level_checks.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/time_series_test_data.h>

#include <SandboxCore/time_series_data.h>

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>
#include <Sandbox/batch_game/TestSpaceShip.h>
#include <Sandbox/batch_game/TestSpaceShipController.h>

#include <CQTest.h>
#include <Engine/GameInstance.h>
#include <Engine/LocalPlayer.h>
#include <Engine/World.h>
#include <Misc/Optional.h>

namespace {
ml::FTestBatchOrchestratorLevelSetup player_ship_death_level_setup{};
}

TEST_CLASS(TestPlayerShipDeath, "Sandbox.LevelTests")
{
    using ThisClass = TestPlayerShipDeath;

    struct FSimulationSample {
        bool player_handle_is_dead{false};
        bool player_actor_is_valid{false};
        bool player_unique_entity_is_alive{false};
    };

    inline static FTimespan const timeout{0, 0, 2};
    static constexpr double kill_time{0.1};
    static constexpr double post_kill_time{0.4};

    ml::FSoftTestAssertions checks{};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};
    TWeakObjectPtr<ATestSpaceShip> player_ship{nullptr};
    FRegistryEntityHandle player_ship_handle{};
    TestEntityUniqueId player_ship_id{};
    ml::TimeSeriesData<FSimulationSample> samples;

    BEFORE_EACH()
    {
        test_driver.Reset();
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        samples = {};
        player_ship_death_level_setup.begin_test(TestCommandBuilder, *TestRunner, checks);
        TestCommandBuilder.Do([this] {
            auto& world{player_ship_death_level_setup.get_world()};
            auto const& config{player_ship_death_level_setup.get_config()};
            player_ship_pre_begin_play(world, config);
            auto* const orchestrator{player_ship_death_level_setup.get_orchestrator()};
            if (checks.is_valid(orchestrator, TEXT("Orchestrator is available"))) {
                player_ship_post_orchestrator_spawn(world, config, *orchestrator);
            }
        });
    }

    AFTER_EACH()
    {
        if (auto* orchestrator{player_ship_death_level_setup.get_orchestrator()};
            IsValid(orchestrator)) {
            orchestrator->clear_end_tick_test_hook();
        }
        player_ship_death_level_setup.end_test();
    }

    AFTER_ALL()
    {
        player_ship_death_level_setup.teardown();
    }

    TEST_METHOD(LethalDamageDestroysPlayerShip)
    {
        TestCommandBuilder.Do([this] { queue_player_ship_death(); })
            .Until([this] { return test_driver->timeline.is_finished(); }, timeout)
            .Then([this] { check_player_ship_death(); });
    }
  private:
    void player_ship_pre_begin_play(UWorld & world, UTestSimulationConfig const& config) {
        auto* const spawned_player_ship{
            ml::spawn_player_ship(world,
                                  config.actor_classes.player_ship_class,
                                  config.simulation_config->player_ship_config.Get())};
        if (!checks.is_valid(spawned_player_ship, TEXT("Player ship is spawned"))) {
            return;
        }

        player_ship = spawned_player_ship;
    }

    void player_ship_post_orchestrator_spawn(
        UWorld & world, UTestSimulationConfig const& config, ATestBatchOrchestrator& orchestrator) {
        if (!checks.is_true(player_ship.IsValid(), TEXT("Player ship is available"))) {
            return;
        }

        orchestrator.set_player_ship(*const_cast<ATestSpaceShip*>(player_ship.Get()));
    }

    void queue_player_ship_death() {
        test_driver =
            ml::TestSimulationDriver::from_world(player_ship_death_level_setup.get_world());
        test_driver->orchestrator.start_simulation();

        auto* const ship{test_driver->orchestrator.get_player_ship()};
        checks.is_valid(ship, TEXT("Player ship is valid"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        player_ship = const_cast<ATestSpaceShip*>(ship);
        player_ship_handle = ship->get_entity_handle();
        player_ship_id = ship->get_unique_id();

        auto const& registry{test_driver->orchestrator.get_entity_registry()};
        checks.is_true(registry.is_valid_handle(player_ship_handle),
                       TEXT("Player ship handle is valid"));
        checks.is_true(registry.is_valid_unique_id(player_ship_id),
                       TEXT("Player ship ID is valid"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        TArray<FRegistryEntityHandle> const targets{player_ship_handle};
        test_driver->timeline
            .then_after(kill_time, [this, targets] { test_driver->queue_kills(targets); })
            .finish_after(post_kill_time);

        ml::reset_and_reserve_time_series(
            test_driver->orchestrator, kill_time + post_kill_time, samples);
        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));
    }

    void on_end_tick(ATestBatchOrchestrator&) {
        auto const& unique_entities{test_driver->registry.get_unique_entities()};

        if (!checks.is_true(unique_entities.alive.IsValidIndex(player_ship_id.id),
                            TEXT("Check player id is valid"))) {
            SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
        }

        samples.add(test_driver->get_time(),
                    FSimulationSample{test_driver->registry.is_valid_dead(player_ship_handle),
                                      IsValid(player_ship.Get()),
                                      static_cast<bool>(unique_entities.alive[player_ship_id.id])});
        test_driver->timeline.tick(test_driver->get_time());
    }

    void check_player_ship_death() {
        ml::check_samples_recorded(
            samples.num(), checks, TEXT("Player-death simulation samples recorded"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        auto const& sample{samples.last_value()};
        checks.is_true(sample.player_handle_is_dead, TEXT("Player ship handle is dead"));
        checks.is_true(!sample.player_actor_is_valid, TEXT("Player ship actor is destroyed"));
        checks.is_true(!sample.player_unique_entity_is_alive, TEXT("Player ship entity is dead"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }
};
