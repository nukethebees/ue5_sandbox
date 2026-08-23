#include <SandboxTests/support/test_setup.h>

#include <SandboxTests/support/SoftTestAssertions.h>

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
#include <Editor.h>
#include <Editor/UnrealEdEngine.h>
#include <HAL/FileManager.h>
#include <LevelEditorSubsystem.h>
#include <Misc/Optional.h>
#include <Misc/Paths.h>
#include <Misc/PackageName.h>
#include <Tests/AutomationEditorCommon.h>
#include <UnrealEdGlobals.h>

namespace {

enum class ETestLevelState : uint8 { Unconstructed, Constructing, Constructed };

struct FTestBatchOrchestratorSetupLevelContext {
    auto construct() -> bool {
        check(state == ETestLevelState::Unconstructed);

        if (IsValid(GUnrealEd->PlayWorld)) {
            GUnrealEd->EndPlayMap();
        }

        map_directory = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("SandboxTestBatchOrchestratorSetupTemp"));
        map_name = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
        auto const map_path{FPaths::Combine(map_directory, map_name)};
        auto const package_name{FPackageName::FilenameToLongPackageName(map_path)};
        auto* const level_editor_subsystem{GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()};
        if (!IsValid(level_editor_subsystem) || !level_editor_subsystem->NewLevel(package_name)) {
            return false;
        }

        spawner = MakeUnique<FMapTestSpawner>(map_directory, map_name);
        state = ETestLevelState::Constructing;
        ++construction_count;
        return true;
    }

    void set_orchestrator(ATestBatchOrchestrator& new_orchestrator) {
        check(state == ETestLevelState::Constructing);
        orchestrator = &new_orchestrator;
        state = ETestLevelState::Constructed;
    }

    void reset() {
        check(state == ETestLevelState::Constructed);
        check(orchestrator.IsValid());
        orchestrator->reset_for_new_level();
        ++reset_count;
    }

    auto get_orchestrator() const -> ATestBatchOrchestrator* { return orchestrator.Get(); }
    auto get_world() const -> UWorld& {
        check(spawner);
        return spawner->GetWorld();
    }

    void teardown() {
        if (IsValid(GUnrealEd->PlayWorld)) {
            GUnrealEd->EndPlayMap();
        }

        FAutomationEditorCommonUtils::CreateNewMap();
        spawner.Reset();
        orchestrator.Reset();

        if (!map_directory.IsEmpty()) {
            IFileManager::Get().DeleteDirectory(*map_directory, false, true);
        }

        map_directory.Reset();
        map_name.Reset();
        state = ETestLevelState::Unconstructed;
        construction_count = 0;
        reset_count = 0;
    }

    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    TWeakObjectPtr<ATestBatchOrchestrator> orchestrator{nullptr};
    FString map_directory{};
    FString map_name{};
    ETestLevelState state{ETestLevelState::Unconstructed};
    int32 construction_count{0};
    int32 reset_count{0};
};

FTestBatchOrchestratorSetupLevelContext level_context{};

} // namespace

TEST_CLASS(TestBatchOrchestratorSetup, "Sandbox.LevelTests")
{
    TOptional<ml::FTestBatchOrchestratorLevelSetup> level_setup{NullOpt};
    ml::FSoftTestAssertions checks{};

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;

        if (level_context.state == ETestLevelState::Unconstructed) {
            if (!TestRunner->TestTrue(TEXT("Reusable test level is constructed"), level_context.construct())) {
                return;
            }

            level_setup.Emplace(*level_context.spawner, *TestRunner, checks);
            level_setup->setup(
                TestCommandBuilder,
                {},
                [](UWorld&, UTestSimulationConfig const&, ATestBatchOrchestrator& orchestrator) {
                    orchestrator.set_start_mode(EOrchestratorStartMode::Paused);
                });
            TestCommandBuilder.Do([this] {
                auto* const orchestrator{level_setup->get_orchestrator()};
                if (!TestRunner->TestNotNull(TEXT("Orchestrator is available for level reuse"), orchestrator)) {
                    return;
                }

                level_context.set_orchestrator(*orchestrator);
            });
        } else if (level_context.state == ETestLevelState::Constructed) {
            if (!TestRunner->TestNotNull(
                    TEXT("Reusable orchestrator is available"), level_context.get_orchestrator())) {
                return;
            }

            level_context.reset();
        } else {
            TestRunner->AddError(TEXT("Reusable test level is still constructing"));
        }
    }

    AFTER_EACH()
    {
        if (level_setup.IsSet()) {
            level_setup->teardown();
            level_setup.Reset();
        }
    }

    AFTER_ALL()
    {
        level_context.teardown();
    }

    TEST_METHOD(SpawnMissingActors)
    {
        TestCommandBuilder.Do([this] {
            auto* const orchestrator{level_context.get_orchestrator()};
            if (!TestRunner->TestNotNull(TEXT("Orchestrator is available"), orchestrator)) {
                return;
            }

            auto& world{level_context.get_world()};

            TestRunner->TestTrue(TEXT("Paused start mode defers orchestrator initialisation"),
                                 orchestrator->get_state() == EOrchestratorState::Uninitialised);
            TestRunner->TestFalse(TEXT("Uninitialised orchestrator does not tick"),
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
            TestRunner->TestNotNull(TEXT("Entity registry is embedded"),
                                    &orchestrator->get_entity_registry());
            TestRunner->TestNotNull(TEXT("Mission manager is embedded"),
                                    &orchestrator->get_mission_manager());
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
            auto* const orchestrator{level_context.get_orchestrator()};
            if (!TestRunner->TestNotNull(TEXT("Orchestrator is available"), orchestrator)) {
                return;
            }

            TestRunner->TestEqual(TEXT("Reusable level is constructed once"),
                                  level_context.construction_count,
                                  1);
            TestRunner->TestTrue(TEXT("Reset leaves the orchestrator uninitialised"),
                                 orchestrator->get_state() == EOrchestratorState::Uninitialised);

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
