#include "test_setup.h"
#include "TestSimulationDriver.h"

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>
#include <Sandbox/batch_game/TestSpaceShip.h>
#include <Sandbox/batch_game/TestSpaceShipController.h>

#include <CQTest.h>
#include <Engine/GameInstance.h>
#include <Engine/LocalPlayer.h>
#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/Optional.h>

TEST_CLASS(TestPlayerShipDeath, "Sandbox.FunctionalTests")
{
    using ThisClass = TestPlayerShipDeath;

    inline static FTimespan const timeout{0, 0, 2};

    ml::FTestBatchOrchestratorLevelSetup level_setup;
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};
    TWeakObjectPtr<ATestSpaceShip> player_ship{nullptr};
    FRegistryEntityHandle player_ship_handle{};
    TestEntityUniqueId player_ship_id{};

    BEFORE_EACH()
    {
        test_driver.Reset();
        level_setup.setup(TestCommandBuilder, *TestRunner);
    }

    AFTER_EACH()
    {
        if (auto* orchestrator{level_setup.get_orchestrator()}; IsValid(orchestrator)) {
            orchestrator->clear_end_tick_test_hook();
        }
        level_setup.teardown();
    }

    TEST_METHOD(LethalDamageDestroysPlayerShip)
    {
        TestCommandBuilder.Do([this] { queue_player_ship_death(); })
            .Until([this] { return test_driver->timeline.is_finished(); }, timeout)
            .Then([this] { check_player_ship_death(); });
    }
  private:
    void queue_player_ship_death() {
        test_driver = ml::TestSimulationDriver::from_world(level_setup.get_world());
        test_driver->timeline.finish_at(0.5);

        auto* const ship{const_cast<ATestSpaceShip*>(&test_driver->get_player_ship())};

        player_ship = ship;
        player_ship_handle = ship->get_entity_handle();
        player_ship_id = ship->get_unique_id();

        TArray<FRegistryEntityHandle> const targets{player_ship_handle};
        test_driver->queue_kills(targets);

        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));
        test_driver->orchestrator.start_simulation();
    }

    void on_end_tick(ATestBatchOrchestrator&) {
        test_driver->timeline.tick(test_driver->get_time());
    }

    void check_player_ship_death() {
        TestRunner->TestTrue(TEXT("Player ship handle is dead"),
                             test_driver->registry.is_valid_dead(player_ship_handle));
        TestRunner->TestFalse(TEXT("Player ship actor is destroyed"), IsValid(player_ship.Get()));

        auto const& unique_entities{test_driver->registry.get_unique_entities()};
        TestRunner->TestFalse(TEXT("Player ship entity is dead"),
                              static_cast<bool>(unique_entities.alive[player_ship_id.id]));
    }
};
