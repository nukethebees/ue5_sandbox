#include "SimulationTestAssets.h"

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/SimulationClockInterface.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestLasers.h>
#include <Sandbox/batch_game/TestMissionManager.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>
#include <Sandbox/batch_game/TestSpaceShip.h>
#include <Sandbox/batch_game/TestStaticTurrets.h>
#include <Sandbox/batch_game/TestTubeSpinners.h>
#include <Sandbox/environment/effects/DelayedNiagaraSpawner.h>

#include <SandboxCoreEngine/actor_utils.h>

#include <Components/MapTestSpawner.h>
#include <CQTest.h>
#include <Editor.h>
#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>

TEST_CLASS(TestBatchOrchestratorSetup, "Sandbox.UnitTests")
{
    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    ATestBatchOrchestrator* orchestrator{nullptr};
    FDelegateHandle map_change_handle{};
    bool actors_spawned{false};

    auto spawn_orchestrator(UWorld& world) -> bool
    {
        auto const* const config{ml::load_default_test_simulation_config()};
        if (!TestRunner->TestNotNull(TEXT("Default test simulation config loads"), config)) {
            return false;
        }
        if (!TestRunner->TestTrue(TEXT("Default test simulation config is valid"),
                                  config->is_valid())) {
            return false;
        }

        auto* const new_orchestrator{world.SpawnActorDeferred<ATestBatchOrchestrator>(
            ATestBatchOrchestrator::StaticClass(), FTransform::Identity)};
        if (!TestRunner->TestNotNull(TEXT("Deferred orchestrator is spawned"), new_orchestrator)) {
            return false;
        }

        new_orchestrator->set_start_mode(EOrchestratorStartMode::PausedInTest);
        new_orchestrator->set_test_config(*config);
        new_orchestrator->spawn_missing_actors();

        UGameplayStatics::FinishSpawningActor(new_orchestrator, FTransform::Identity);
        return true;
    }

    void resolve_orchestrator()
    {
        if (!TestRunner->TestTrue(TEXT("Actors are spawned from the map-change callback"),
                                  actors_spawned)) {
            return;
        }

        orchestrator = ml::get_first_actor<ATestBatchOrchestrator>(spawner->GetWorld());

        TestRunner->TestNotNull(TEXT("PIE orchestrator is available"), orchestrator);
    }

    BEFORE_EACH()
    {
        orchestrator = nullptr;
        actors_spawned = false;
        map_change_handle = FEditorDelegates::MapChange.AddLambda([this](uint32 const flags) {
            if (actors_spawned || !(flags & MapChangeEventFlags::NewMap)) {
                return;
            }

            if (!TestRunner->TestNotNull(TEXT("Editor is available"), GEditor)) {
                return;
            }

            auto* const world{GEditor->GetEditorWorldContext().World()};
            if (!TestRunner->TestNotNull(TEXT("Editor world is available"), world)) {
                return;
            }

            actors_spawned = spawn_orchestrator(*world);
        });

        spawner = FMapTestSpawner::CreateFromTempLevel(TestCommandBuilder);
        ASSERT_THAT(IsNotNull(spawner));
        spawner->AddWaitUntilLoadedCommand(TestRunner);
        TestCommandBuilder.Do([this] { resolve_orchestrator(); });
    }

    AFTER_EACH()
    {
        FEditorDelegates::MapChange.Remove(map_change_handle);
        map_change_handle.Reset();
    }

    TEST_METHOD(SpawnMissingActors)
    {
        TestCommandBuilder.Do([this] {
            if (!TestRunner->TestNotNull(TEXT("Orchestrator is available"), orchestrator)) {
                return;
            }

            auto& world{spawner->GetWorld()};

            TestRunner->TestTrue(TEXT("Orchestrator begins paused in an automation test"),
                                 orchestrator->get_state() == EOrchestratorState::Paused);
            TestRunner->TestFalse(TEXT("Paused orchestrator does not tick"),
                                  orchestrator->IsActorTickEnabled());

            TestRunner->TestEqual(TEXT("One lasers actor exists"),
                                  ml::count_actors<ATestLasers>(world),
                                  1);
            TestRunner->TestEqual(TEXT("One capital ships actor exists"),
                                  ml::count_actors<ATestCapitalShips>(world),
                                  1);
            TestRunner->TestEqual(TEXT("One fighters actor exists"),
                                  ml::count_actors<ATestCapitalShipFighters>(world),
                                  1);
            TestRunner->TestEqual(TEXT("One turrets actor exists"),
                                  ml::count_actors<ATestStaticTurrets>(world),
                                  1);
            TestRunner->TestEqual(TEXT("One spinners actor exists"),
                                  ml::count_actors<ATestTubeSpinners>(world),
                                  1);
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
            TestRunner->TestEqual(TEXT("Duration periods round up"),
                                  clock.duration_to_tick_period(0.025),
                                  uint64{2});
        });
    }
};
