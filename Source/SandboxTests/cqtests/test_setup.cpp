#include "test_setup.h"

#include "SimulationTestAssets.h"

#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>
#include <Sandbox/core/SandboxDeveloperSettings.h>

#include <SandboxCoreEngine/actor_utils.h>

#include <SandboxCore/error_msg.h>

#include <Commands/TestCommandBuilder.h>
#include <Components/MapTestSpawner.h>
#include <CoreMinimal.h>
#include <Editor.h>
#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>

namespace ml {
auto get_editor_world() -> std::expected<UWorld*, FErrorMsg> {
    if (!GEditor) {
        return std::unexpected(FErrorMsg{TEXT("GEditor is nullptr")});
    }

    auto* const world{GEditor->GetEditorWorldContext().World()};
    if (!IsValid(world)) {
        return std::unexpected(FErrorMsg{TEXT("Editor world is invalid")});
    }

    return world;
}

FTestBatchOrchestratorLevelSetup::~FTestBatchOrchestratorLevelSetup() = default;

void FTestBatchOrchestratorLevelSetup::setup(FTestCommandBuilder& command_builder,
                                             FAutomationTestBase& new_test_runner,
                                             FConfigureBatchTestLevel new_configure_level) {
    test_runner = &new_test_runner;
    checks.test_runner = test_runner;
    checks.all_passed = true;
    configure_level = MoveTemp(new_configure_level);
    orchestrator = nullptr;
    actors_spawned = false;

    map_change_handle = FEditorDelegates::MapChange.AddLambda([this](uint32 const flags) {
        if (actors_spawned || !(flags & MapChangeEventFlags::NewMap)) {
            return;
        }

        if (!checks.not_nullptr(GEditor, TEXT("Editor is available"))) {
            return;
        }

        auto* const world{GEditor->GetEditorWorldContext().World()};
        if (!checks.not_nullptr(world, TEXT("Editor world is available"))) {
            return;
        }

        actors_spawned = spawn_orchestrator(*world);
    });

    spawner = FMapTestSpawner::CreateFromTempLevel(command_builder);
    if (!checks.not_nullptr(spawner.Get(), TEXT("Temporary level spawner is available"))) {
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
    checks.test_runner = nullptr;
    configure_level = {};
    actors_spawned = false;
}

auto FTestBatchOrchestratorLevelSetup::get_world() const -> UWorld& {
    check(spawner);
    return spawner->GetWorld();
}

auto FTestBatchOrchestratorLevelSetup::spawn_orchestrator(UWorld& world) -> bool {
    auto const* const config{load_default_test_simulation_config()};
    if (!checks.not_nullptr(config, TEXT("Default test simulation config loads"))) {
        return false;
    }
    if (!checks.is_true(config->is_valid(), TEXT("Default test simulation config is valid"))) {
        return false;
    }

    if (configure_level) {
        configure_level(world, *config);
    }

    auto* const new_orchestrator{world.SpawnActorDeferred<ATestBatchOrchestrator>(
        ATestBatchOrchestrator::StaticClass(), FTransform::Identity)};
    if (!checks.is_valid(new_orchestrator, TEXT("Deferred orchestrator is spawned"))) {
        return false;
    }

    new_orchestrator->set_start_mode(EOrchestratorStartMode::PausedInTest);
    new_orchestrator->set_test_config(*config);
    new_orchestrator->spawn_missing_actors();

    UGameplayStatics::FinishSpawningActor(new_orchestrator, FTransform::Identity);
    return true;
}

void FTestBatchOrchestratorLevelSetup::resolve_orchestrator() {
    if (!checks.is_true(actors_spawned, TEXT("Actors are spawned from the map-change callback"))) {
        return;
    }

    orchestrator = ml::get_first_actor<ATestBatchOrchestrator>(spawner->GetWorld());
    checks.is_valid(orchestrator, TEXT("PIE orchestrator is available"));
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
