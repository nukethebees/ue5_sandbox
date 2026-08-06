#include "test_setup.h"

#include "SimulationTestAssets.h"

#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>
#include <Sandbox/core/SandboxDeveloperSettings.h>
#include <SandboxTests/cqtests/SoftTestAssertions.h>

#include <SandboxCoreEngine/actor_utils.h>

#include <Commands/TestCommandBuilder.h>
#include <Components/MapTestSpawner.h>
#include <CoreMinimal.h>
#include <Editor.h>
#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>

namespace ml {
FTestBatchOrchestratorLevelSetup::~FTestBatchOrchestratorLevelSetup() = default;

void FTestBatchOrchestratorLevelSetup::setup(FTestCommandBuilder& command_builder,
                                             FAutomationTestBase& new_test_runner,
                                             FConfigureBatchTestLevel new_configure_level) {
    test_runner = &new_test_runner;
    configure_level = MoveTemp(new_configure_level);
    orchestrator = nullptr;
    actors_spawned = false;

    map_change_handle = FEditorDelegates::MapChange.AddLambda([this](uint32 const flags) {
        if (actors_spawned || !(flags & MapChangeEventFlags::NewMap)) {
            return;
        }

        if (!test_runner->TestNotNull(TEXT("Editor is available"), GEditor)) {
            return;
        }

        auto* const world{GEditor->GetEditorWorldContext().World()};
        if (!test_runner->TestNotNull(TEXT("Editor world is available"), world)) {
            return;
        }

        actors_spawned = spawn_orchestrator(*world);
    });

    spawner = FMapTestSpawner::CreateFromTempLevel(command_builder);
    test_runner->TestNotNull(TEXT("Temporary level spawner is available"), spawner.Get());
    if (!spawner) {
        return;
    }

    spawner->AddWaitUntilLoadedCommand(test_runner);
    command_builder.Do([this] { resolve_orchestrator(); });
}

void FTestBatchOrchestratorLevelSetup::teardown() {
    if (map_change_handle.IsValid()) {
        FEditorDelegates::MapChange.Remove(map_change_handle);
        map_change_handle.Reset();
    }

    if (IsValid(orchestrator)) {
        orchestrator->pause_simulation();
    }

    orchestrator = nullptr;
    test_runner = nullptr;
    configure_level = {};
    actors_spawned = false;
}

auto FTestBatchOrchestratorLevelSetup::get_world() const -> UWorld& {
    check(spawner);
    return spawner->GetWorld();
}

auto FTestBatchOrchestratorLevelSetup::spawn_orchestrator(UWorld& world) -> bool {
    auto const* const config{load_default_test_simulation_config()};
    if (!test_runner->TestNotNull(TEXT("Default test simulation config loads"), config)) {
        return false;
    }
    if (!test_runner->TestTrue(TEXT("Default test simulation config is valid"),
                               config->is_valid())) {
        return false;
    }

    if (configure_level) {
        configure_level(world, *config);
    }

    auto* const new_orchestrator{world.SpawnActorDeferred<ATestBatchOrchestrator>(
        ATestBatchOrchestrator::StaticClass(), FTransform::Identity)};
    if (!test_runner->TestNotNull(TEXT("Deferred orchestrator is spawned"), new_orchestrator)) {
        return false;
    }

    new_orchestrator->set_start_mode(EOrchestratorStartMode::PausedInTest);
    new_orchestrator->set_test_config(*config);
    new_orchestrator->spawn_missing_actors();

    UGameplayStatics::FinishSpawningActor(new_orchestrator, FTransform::Identity);
    return true;
}

void FTestBatchOrchestratorLevelSetup::resolve_orchestrator() {
    if (!test_runner->TestTrue(TEXT("Actors are spawned from the map-change callback"),
                               actors_spawned)) {
        return;
    }

    orchestrator = ml::get_first_actor<ATestBatchOrchestrator>(spawner->GetWorld());
    test_runner->TestNotNull(TEXT("PIE orchestrator is available"), orchestrator);
}

auto level_test_setup(FString const& map_directory,
                      FString const& map_name,
                      FAutomationTestBase* test_runner,
                      FSoftTestAssertions& checks) -> TUniquePtr<FMapTestSpawner> {
    auto spawner{MakeUnique<FMapTestSpawner>(map_directory, map_name)};
    spawner->AddWaitUntilLoadedCommand(test_runner);

    checks.test_runner = test_runner;
    checks.all_passed = true;

#if WITH_EDITOR
    auto const* settings{GetDefault<USandboxDeveloperSettings>()};
    checks.log_successful_assertions = settings->log_successful_assertions;
#endif

    return spawner;
}

auto level_test_setup(FString const& map_name,
                      FAutomationTestBase* test_runner,
                      FSoftTestAssertions& checks) -> TUniquePtr<FMapTestSpawner> {
    return ml::level_test_setup(
        TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/test_levels/functional_tests"),
        map_name,
        test_runner,
        checks);
}
}
