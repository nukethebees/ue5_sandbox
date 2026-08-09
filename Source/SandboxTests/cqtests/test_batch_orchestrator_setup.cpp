#include "test_setup.h"

#include <SandboxTests/cqtests/SoftTestAssertions.h>

#include <Sandbox/batch_game/SimulationClockInterface.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestLasers.h>
#include <Sandbox/batch_game/TestMissionManager.h>
#include <Sandbox/batch_game/TestSpaceShip.h>
#include <Sandbox/batch_game/TestStaticTurrets.h>
#include <Sandbox/batch_game/TestTubeSpinners.h>
#include <Sandbox/environment/effects/DelayedNiagaraSpawner.h>

#include <SandboxCoreEngine/actor_utils.h>

#include <Components/MapTestSpawner.h>
#include <CQTest.h>
#include <Misc/Optional.h>

TEST_CLASS(TestBatchOrchestratorSetup, "Sandbox.UnitTests")
{
    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    TOptional<ml::FTestBatchOrchestratorLevelSetup> level_setup{NullOpt};
    ml::FSoftTestAssertions checks{};

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        spawner = FMapTestSpawner::CreateFromTempLevel(TestCommandBuilder);
        level_setup.Emplace(*spawner, *TestRunner, checks);
        level_setup->setup(TestCommandBuilder);
    }

    AFTER_EACH()
    {
        level_setup->teardown();
        level_setup.Reset();
        spawner.Reset();
    }

    TEST_METHOD(SpawnMissingActors)
    {
        TestCommandBuilder.Do([this] {
            auto* const orchestrator{level_setup->get_orchestrator()};
            if (!TestRunner->TestNotNull(TEXT("Orchestrator is available"), orchestrator)) {
                return;
            }

            auto& world{level_setup->get_world()};

            TestRunner->TestTrue(TEXT("Orchestrator begins paused in an automation test"),
                                 orchestrator->get_state() == EOrchestratorState::Paused);
            TestRunner->TestFalse(TEXT("Paused orchestrator does not tick"),
                                  orchestrator->IsActorTickEnabled());

            TestRunner->TestEqual(
                TEXT("One lasers actor exists"), ml::count_actors<ATestLasers>(world), 1);
            TestRunner->TestEqual(TEXT("One capital ships actor exists"),
                                  ml::count_actors<ATestCapitalShips>(world),
                                  1);
            TestRunner->TestEqual(TEXT("One fighters actor exists"),
                                  ml::count_actors<ATestCapitalShipFighters>(world),
                                  1);
            TestRunner->TestEqual(
                TEXT("One turrets actor exists"), ml::count_actors<ATestStaticTurrets>(world), 1);
            TestRunner->TestEqual(
                TEXT("One spinners actor exists"), ml::count_actors<ATestTubeSpinners>(world), 1);
            TestRunner->TestEqual(TEXT("One entity registry exists"),
                                  ml::count_actors<ATestEntityRegistry>(world),
                                  1);
            TestRunner->TestEqual(TEXT("One mission manager exists"),
                                  ml::count_actors<ATestMissionManager>(world),
                                  1);
            TestRunner->TestEqual(TEXT("One Niagara spawner exists"),
                                  ml::count_actors<ADelayedNiagaraSpawner>(world),
                                  1);

            TestRunner->TestNotNull(TEXT("Lasers are bound"), orchestrator->get_lasers());
            TestRunner->TestNotNull(TEXT("Capital ships are bound"),
                                    orchestrator->get_capital_ships());
            TestRunner->TestNotNull(TEXT("Fighters are bound"),
                                    orchestrator->get_capital_ship_fighters());
            TestRunner->TestNotNull(TEXT("Turrets are bound"), orchestrator->get_turrets());
            TestRunner->TestNotNull(TEXT("Spinners are bound"), orchestrator->get_spinners());
            TestRunner->TestNotNull(TEXT("Entity registry is bound"),
                                    orchestrator->get_entity_registry());
            TestRunner->TestNotNull(TEXT("Mission manager is bound"),
                                    orchestrator->get_mission_manager());
            TestRunner->TestNotNull(TEXT("Niagara spawner is bound"),
                                    orchestrator->get_niagara_spawner());

            auto const completed_ticks{orchestrator->get_completed_ticks()};
            orchestrator->start_simulation();

            TestRunner->TestTrue(TEXT("Starting transitions the orchestrator to running"),
                                 orchestrator->get_state() == EOrchestratorState::Running);
            TestRunner->TestTrue(TEXT("Running orchestrator ticks"),
                                 orchestrator->IsActorTickEnabled());
            TestRunner->TestEqual(TEXT("Starting does not immediately advance simulation"),
                                  orchestrator->get_completed_ticks(),
                                  completed_ticks);
        });
    }

    TEST_METHOD(SimulationClockConversions)
    {
        TestCommandBuilder.Do([this] {
            auto* const orchestrator{level_setup->get_orchestrator()};
            if (!TestRunner->TestNotNull(TEXT("Orchestrator is available"), orchestrator)) {
                return;
            }

            ml::test_batch_orchestrator::SimulationClockInterface clock;
            clock.bind(*orchestrator);

            TestRunner->TestEqual(TEXT("Tick-rate frequency has a one-tick period"),
                                  clock.frequency_to_tick_period(60.0),
                                  uint64{1});
            TestRunner->TestEqual(TEXT("Frequency periods round up"),
                                  clock.frequency_to_tick_period(24.0),
                                  uint64{3});
            TestRunner->TestEqual(TEXT("Above-tick-rate frequency has a one-tick period"),
                                  clock.frequency_to_tick_period(120.0),
                                  uint64{1});
            TestRunner->TestEqual(TEXT("Zero duration has a zero-tick period"),
                                  clock.duration_to_tick_period(0.0),
                                  uint64{0});
            TestRunner->TestEqual(TEXT("One second uses the configured tick rate"),
                                  clock.duration_to_tick_period(1.0),
                                  uint64{60});
            TestRunner->TestEqual(
                TEXT("Duration periods round up"), clock.duration_to_tick_period(0.025), uint64{2});
        });
    }
};
