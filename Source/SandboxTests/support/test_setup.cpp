#include "test_setup.h"

#include "SimulationTestAssets.h"

#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>
#include <SandboxGameShared/core/SandboxDeveloperSettings.h>

#include <SandboxCoreEngine/actor_utils.h>

#include <SandboxCore/error_msg.h>

#include <Commands/TestCommandBuilder.h>
#include <Components/BoxComponent.h>
#include <Components/MapTestSpawner.h>
#include <CoreMinimal.h>
#include <Editor.h>
#include <Engine/World.h>
#include <GameFramework/Actor.h>
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

auto spawn_visibility_blocker(UWorld& world,
                              FVector const location,
                              FVector const extent,
                              FName const name) -> AActor* {
    auto* const blocker{world.SpawnActor<AActor>(
        AActor::StaticClass(), location, FRotator::ZeroRotator)};
    if (!IsValid(blocker)) {
        return nullptr;
    }

    auto* const collision{NewObject<UBoxComponent>(blocker, name)};
    if (!IsValid(collision)) {
        return nullptr;
    }

    blocker->AddInstanceComponent(collision);
    blocker->SetRootComponent(collision);
    collision->SetBoxExtent(extent);
    collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    collision->SetCollisionObjectType(ECC_WorldStatic);
    collision->SetCollisionResponseToAllChannels(ECR_Ignore);
    collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    collision->RegisterComponent();
    return blocker;
}

FTestBatchOrchestratorLevelSetup::~FTestBatchOrchestratorLevelSetup() = default;

FTestBatchOrchestratorLevelSetup::FTestBatchOrchestratorLevelSetup(
    FMapTestSpawner& new_spawner,
    FAutomationTestBase& new_test_runner,
    FSoftTestAssertions& new_checks)
    : spawner{&new_spawner}
    , test_runner{&new_test_runner}
    , checks{&new_checks} {}

void FTestBatchOrchestratorLevelSetup::setup(
    FTestCommandBuilder& command_builder,
    FConfigureBatchTestLevel new_configure_level,
    FConfigureBatchTestOrchestrator new_configure_orchestrator) {
    check(spawner);
    check(test_runner);
    check(checks);

    configure_level = MoveTemp(new_configure_level);
    configure_orchestrator = MoveTemp(new_configure_orchestrator);
    orchestrator = nullptr;
    actors_spawned = false;

    map_change_handle = FEditorDelegates::MapChange.AddLambda([this](uint32 const flags) {
        if (actors_spawned || !(flags & MapChangeEventFlags::NewMap)) {
            return;
        }

        check(checks);
        if (!checks->not_nullptr(GEditor, TEXT("Editor is available"))) {
            return;
        }

        auto* const world{GEditor->GetEditorWorldContext().World()};
        if (!checks->not_nullptr(world, TEXT("Editor world is available"))) {
            return;
        }

        actors_spawned = spawn_orchestrator(*world);
    });

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
    configure_orchestrator = {};
    actors_spawned = false;
}

auto FTestBatchOrchestratorLevelSetup::get_world() const -> UWorld& {
    check(spawner);
    return spawner->GetWorld();
}

auto FTestBatchOrchestratorLevelSetup::spawn_orchestrator(UWorld& world) -> bool {
    auto const* const config{load_default_test_simulation_config()};
    if (!checks->not_nullptr(config, TEXT("Default test simulation config loads"))) {
        return false;
    }
    if (!checks->is_true(config->is_valid(), TEXT("Default test simulation config is valid"))) {
        return false;
    }

    if (configure_level) {
        configure_level(world, *config);
    }

    auto* const new_orchestrator{world.SpawnActorDeferred<ATestBatchOrchestrator>(
        ATestBatchOrchestrator::StaticClass(), FTransform::Identity)};
    if (!checks->is_valid(new_orchestrator, TEXT("Deferred orchestrator is spawned"))) {
        return false;
    }

    new_orchestrator->set_start_mode(EOrchestratorStartMode::PausedInTest);
    new_orchestrator->set_test_config(*config);
    new_orchestrator->spawn_missing_actors();

    if (configure_orchestrator) {
        configure_orchestrator(world, *config, *new_orchestrator);
    }

    auto* const finished_orchestrator{
        UGameplayStatics::FinishSpawningActor(new_orchestrator, FTransform::Identity)};
    return checks->is_valid(finished_orchestrator, TEXT("Orchestrator finish spawning succeeded"));
}

void FTestBatchOrchestratorLevelSetup::resolve_orchestrator() {
    if (!checks->is_true(actors_spawned, TEXT("Actors are spawned from the map-change callback"))) {
        return;
    }

    orchestrator = ml::get_first_actor<ATestBatchOrchestrator>(spawner->GetWorld());
    checks->is_valid(orchestrator, TEXT("PIE orchestrator is available"));
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
