#include "test_setup.h"

#include "SimulationTestAssets.h"

#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

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
    collision->SetMobility(EComponentMobility::Static);
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

void FTestBatchOrchestratorLevelSetup::end_test() {
    if (!orchestrator.IsValid()) {
        return;
    }

    orchestrator->clear_end_tick_test_hook();
    orchestrator->pause_simulation();

    auto* const player_ship{const_cast<ATestSpaceShip*>(orchestrator->get_player_ship())};
    if (IsValid(player_ship)) {
        player_ship->Destroy();
    }
    orchestrator->clear_player_ship();
    orchestrator->reset_for_new_level();

    reset_test_configuration();
}

void FTestBatchOrchestratorLevelSetup::reset_test_configuration() {
    check(orchestrator.IsValid());
    auto& mission_manager{orchestrator->get_mission_manager()};
    mission_manager.set_mission_mode(ETestMissionMode::None);
    mission_manager.set_target_time(60.f);
    mission_manager.set_kill_target(5);
    mission_manager.set_save_mission_results(false);
}

void FTestBatchOrchestratorLevelSetup::teardown() {
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

auto FTestBatchOrchestratorLevelSetup::get_config() const -> USpaceGameLevelConfig const& {
    check(IsValid(config));
    return *config;
}

auto FTestBatchOrchestratorLevelSetup::get_world() const -> UWorld& {
    check(spawner);
    return spawner->GetWorld();
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
    config = load_default_level_config();
    if (!IsValid(config) || !config->is_valid()) {
        return false;
    }

    auto* const new_orchestrator{world.SpawnActorDeferred<ATestBatchOrchestrator>(
        ATestBatchOrchestrator::StaticClass(), FTransform::Identity)};
    if (!IsValid(new_orchestrator)) {
        return false;
    }

    new_orchestrator->set_start_mode(EOrchestratorStartMode::PausedInTest);
    new_orchestrator->set_level_config(*config);
    new_orchestrator->spawn_missing_actors();
    auto* const finished_orchestrator{
        UGameplayStatics::FinishSpawningActor(new_orchestrator, FTransform::Identity)};
    if (!IsValid(finished_orchestrator)) {
        return false;
    }

    orchestrator = Cast<ATestBatchOrchestrator>(finished_orchestrator);
    state = ETestLevelState::Constructed;
    reset_test_configuration();
    return true;
}

}
