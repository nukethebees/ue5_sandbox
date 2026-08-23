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
#include <Editor/UnrealEdEngine.h>
#include <Engine/World.h>
#include <GameFramework/Actor.h>
#include <HAL/FileManager.h>
#include <Kismet/GameplayStatics.h>
#include <LevelEditorSubsystem.h>
#include <Misc/PackageName.h>
#include <Misc/Paths.h>
#include <Tests/AutomationEditorCommon.h>
#include <UnrealEdGlobals.h>

namespace ml {
auto spawn_visibility_blocker(UWorld& world, FTransform const& transform, FName const name)
    -> AActor* {
    auto* const blocker{world.SpawnActor<AActor>(AActor::StaticClass(), transform)};
    if (!IsValid(blocker)) {
        return nullptr;
    }

    auto* const collision{NewObject<UBoxComponent>(blocker, name)};
    if (!IsValid(collision)) {
        blocker->Destroy();
        return nullptr;
    }

    blocker->AddInstanceComponent(collision);
    blocker->SetRootComponent(collision);
    collision->SetBoxExtent(FVector{100.f, 1000.f, 1000.f});
    collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    collision->SetCollisionObjectType(ECC_WorldStatic);
    collision->SetCollisionResponseToAllChannels(ECR_Ignore);
    collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    collision->RegisterComponent();
    blocker->SetActorTransform(transform);
    return blocker;
}

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

void FTestBatchOrchestratorLevelSetup::begin_test(FTestCommandBuilder& command_builder,
                                                  FAutomationTestBase& test_runner,
                                                  FSoftTestAssertions& checks) {
    if (state == ETestLevelState::Unconstructed) {
        if (!checks.is_true(construct_level(), TEXT("Reusable temporary level is constructed"))) {
            return;
        }

        spawner->AddWaitUntilLoadedCommand(&test_runner);
        command_builder.Do([this, &checks] {
            if (!spawn_orchestrator(get_world())) {
                checks.all_passed = false;
            }
        });
        return;
    }

    checks.is_true(state == ETestLevelState::Constructed,
                   TEXT("Reusable temporary level is constructed before use"));
    checks.is_valid(orchestrator.Get(), TEXT("Reusable test orchestrator is valid"));
}

FTestBatchOrchestratorLevelSetup::FTestBatchOrchestratorLevelSetup(
    FMapTestSpawner& new_spawner,
    FAutomationTestBase& new_test_runner,
    FSoftTestAssertions& new_checks)
    : legacy_spawner{&new_spawner}
    , legacy_test_runner{&new_test_runner}
    , legacy_checks{&new_checks} {}

void FTestBatchOrchestratorLevelSetup::end_test() {
    if (!orchestrator.IsValid()) {
        return;
    }

    orchestrator->pause_simulation();
    orchestrator->reset_for_new_level();
}

void FTestBatchOrchestratorLevelSetup::setup(
    FTestCommandBuilder& command_builder,
    FConfigureBatchTestLevel new_configure_level,
    FConfigureBatchTestOrchestrator new_configure_orchestrator) {
    check(legacy_spawner);
    check(legacy_test_runner);
    check(legacy_checks);

    legacy_configure_level = MoveTemp(new_configure_level);
    legacy_configure_orchestrator = MoveTemp(new_configure_orchestrator);
    legacy_spawner->AddWaitUntilLoadedCommand(legacy_test_runner);
    command_builder.Do([this] {
        auto& world{legacy_spawner->GetWorld()};
        auto const* const loaded_config{load_default_test_simulation_config()};
        if (!legacy_checks->not_nullptr(loaded_config,
                                        TEXT("Default test simulation config loads")) ||
            !legacy_checks->is_true(loaded_config->is_valid(),
                                    TEXT("Default test simulation config is valid"))) {
            return;
        }

        if (legacy_configure_level) {
            legacy_configure_level(world, *loaded_config);
        }

        auto* const new_orchestrator{world.SpawnActorDeferred<ATestBatchOrchestrator>(
            ATestBatchOrchestrator::StaticClass(), FTransform::Identity)};
        if (!legacy_checks->is_valid(new_orchestrator, TEXT("Deferred orchestrator is spawned"))) {
            return;
        }

        new_orchestrator->set_start_mode(EOrchestratorStartMode::PausedInTest);
        new_orchestrator->set_test_config(*loaded_config);
        new_orchestrator->spawn_missing_actors();
        if (legacy_configure_orchestrator) {
            legacy_configure_orchestrator(world, *loaded_config, *new_orchestrator);
        }

        orchestrator = Cast<ATestBatchOrchestrator>(
            UGameplayStatics::FinishSpawningActor(new_orchestrator, FTransform::Identity));
        legacy_checks->is_valid(orchestrator.Get(), TEXT("Orchestrator finish spawning succeeded"));
    });
}

void FTestBatchOrchestratorLevelSetup::teardown() {
    if (legacy_spawner) {
        if (orchestrator.IsValid()) {
            orchestrator->pause_simulation();
        }

        orchestrator.Reset();
        legacy_spawner = nullptr;
        legacy_test_runner = nullptr;
        legacy_checks = nullptr;
        legacy_configure_level = {};
        legacy_configure_orchestrator = {};
        return;
    }

    if (IsValid(GUnrealEd->PlayWorld)) {
        GUnrealEd->EndPlayMap();
    }

    FAutomationEditorCommonUtils::CreateNewMap();
    spawner.Reset();
    orchestrator.Reset();
    config = nullptr;

    if (!map_directory.IsEmpty()) {
        IFileManager::Get().DeleteDirectory(*map_directory, false, true);
    }

    map_directory.Reset();
    map_name.Reset();
    state = ETestLevelState::Unconstructed;
    construction_count = 0;
}

auto FTestBatchOrchestratorLevelSetup::get_config() const -> UTestSimulationConfig const& {
    check(IsValid(config));
    return *config;
}

auto FTestBatchOrchestratorLevelSetup::get_world() const -> UWorld& {
    check(spawner || legacy_spawner);
    return spawner ? spawner->GetWorld() : legacy_spawner->GetWorld();
}

auto FTestBatchOrchestratorLevelSetup::construct_level() -> bool {
    check(state == ETestLevelState::Unconstructed);

    if (IsValid(GUnrealEd->PlayWorld)) {
        GUnrealEd->EndPlayMap();
    }

    map_directory = FPaths::Combine(FPaths::ProjectContentDir(),
                                    TEXT("SandboxReusableTestLevels"),
                                    FGuid::NewGuid().ToString(EGuidFormats::Digits));
    map_name = TEXT("level");
    auto const package_name{
        FPackageName::FilenameToLongPackageName(FPaths::Combine(map_directory, map_name))};
    auto* const level_editor_subsystem{GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()};
    if (!IsValid(level_editor_subsystem) || !level_editor_subsystem->NewLevel(package_name)) {
        return false;
    }

    spawner = MakeUnique<FMapTestSpawner>(map_directory, map_name);
    state = ETestLevelState::Constructing;
    ++construction_count;
    return true;
}

auto FTestBatchOrchestratorLevelSetup::spawn_orchestrator(UWorld& world) -> bool {
    config = load_default_test_simulation_config();
    if (!IsValid(config) || !config->is_valid()) {
        return false;
    }

    auto* const new_orchestrator{world.SpawnActorDeferred<ATestBatchOrchestrator>(
        ATestBatchOrchestrator::StaticClass(), FTransform::Identity)};
    if (!IsValid(new_orchestrator)) {
        return false;
    }

    new_orchestrator->set_start_mode(EOrchestratorStartMode::PausedInTest);
    new_orchestrator->set_test_config(*config);
    new_orchestrator->spawn_missing_actors();
    auto* const finished_orchestrator{
        UGameplayStatics::FinishSpawningActor(new_orchestrator, FTransform::Identity)};
    if (!IsValid(finished_orchestrator)) {
        return false;
    }

    orchestrator = Cast<ATestBatchOrchestrator>(finished_orchestrator);
    state = ETestLevelState::Constructed;
    return true;
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
