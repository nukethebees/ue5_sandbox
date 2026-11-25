#include "SandboxEditor/levels/S7LevelPlayableSetup.h"

#include "SandboxEditor/levels/S7LevelAuthoringActors.h"
#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringMode.h"
#include "SandboxEditor/levels/S7LevelAuthoringSession.h"
#include "SandboxEditor/logging/SandboxEditorLogCategories.h"
#include <ioj/levels/launch_options.h>
#include <Sandbox/environment/effects/ShipPostProcessing.h>
#include <SpaceGame/simulation/LevelSimulationBuilder.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxShaders/GpuStarfield/GpuStarfieldActor.h>

#include <Editor.h>
#include <Engine/Level.h>
#include <Engine/World.h>
#include <EngineUtils.h>
#include <GameFramework/GameModeBase.h>
#include <GameFramework/WorldSettings.h>
#include <ScopedTransaction.h>
#include <UObject/StrongObjectPtr.h>

namespace ioj::editor {
namespace s7_level_playable_setup_detail {
inline constexpr TCHAR game_mode_class_path[]{
    TEXT("/Game/GameModes/BP_SpaceShipGameMode.BP_SpaceShipGameMode_C")};

}

auto compile_playable_s7_definition(levels::LevelDefinition const& definition,
                                    USpaceGameLevelConfig const& resources)
    -> S7AuthoringResult<sim::LevelSimInitData> {
    levels::LaunchOptions const options;
    auto build{prepare_level_simulation(
        {.config = resources, .launch_options = options, .definition = definition})};
    if (!build) {
        return S7AuthoringResult<sim::LevelSimInitData>{std::unexpect, build.error().format()};
    }
    return std::move(*build);
}

auto set_up_playable_s7_level(ULevel& level, AS7LevelAuthoringDocument& document)
    -> S7AuthoringResult<FString> {
    using Result = S7AuthoringResult<FString>;

    auto* const world{level.OwningWorld.Get()};
    auto* const world_settings{IsValid(world) ? world->GetWorldSettings() : nullptr};
    if (!GEditor || !IsValid(world) || world->GetCurrentLevel() != &level ||
        !IsValid(world_settings) || document.GetLevel() != &level) {
        return Result{std::unexpect, TEXT("The current editor level is unavailable.")};
    }
    auto* const game_mode_class{
        LoadClass<AGameModeBase>(nullptr, s7_level_playable_setup_detail::game_mode_class_path)};
    if (!IsValid(game_mode_class)) {
        return Result{std::unexpect, TEXT("The Space Ship game mode asset is unavailable.")};
    }

    ATestBatchOrchestrator* orchestrator{};
    AGpuStarfieldActor* starfield{};
    AShipPostProcessing* post_processing{};
    for (auto const actor_ptr : level.Actors) {
        auto* const actor{actor_ptr.Get()};
        if (!IsValid(actor)) {
            continue;
        }
        if (auto* const candidate{Cast<ATestBatchOrchestrator>(actor)}) {
            if (orchestrator) {
                return Result{std::unexpect,
                              TEXT("The level contains multiple batch orchestrators.")};
            }
            orchestrator = candidate;
        }
        if (auto* const candidate{Cast<AGpuStarfieldActor>(actor)}) {
            if (starfield) {
                return Result{std::unexpect, TEXT("The level contains multiple GPU starfields.")};
            }
            starfield = candidate;
        }
        if (auto* const candidate{Cast<AShipPostProcessing>(actor)}) {
            if (post_processing) {
                return Result{std::unexpect,
                              TEXT("The level contains multiple ship post-processing actors.")};
            }
            post_processing = candidate;
        }
    }

    FScopedTransaction transaction{
        NSLOCTEXT("S7LevelAuthoring", "SetUpPlayableLevel", "Set Up Playable S7 Level")};
    auto* new_orchestrator{orchestrator};
    if (!new_orchestrator) {
        new_orchestrator =
            Cast<ATestBatchOrchestrator>(GEditor->AddActor(&level,
                                                           ATestBatchOrchestrator::StaticClass(),
                                                           FTransform::Identity,
                                                           true,
                                                           RF_Transactional,
                                                           false));
        if (!IsValid(new_orchestrator)) {
            transaction.Cancel();
            return Result{std::unexpect, TEXT("Could not create the batch orchestrator.")};
        }
    }
    auto* new_starfield{starfield};
    if (!new_starfield) {
        new_starfield =
            Cast<AGpuStarfieldActor>(GEditor->AddActor(&level,
                                                       AGpuStarfieldActor::StaticClass(),
                                                       FTransform::Identity,
                                                       true,
                                                       RF_Transactional,
                                                       false));
    }
    if (!IsValid(new_starfield)) {
        if (!orchestrator) {
            new_orchestrator->Destroy();
        }
        transaction.Cancel();
        return Result{std::unexpect, TEXT("Could not create the GPU starfield.")};
    }
    if (!post_processing && !IsValid(GEditor->AddActor(&level,
                                                       AShipPostProcessing::StaticClass(),
                                                       FTransform::Identity,
                                                       true,
                                                       RF_Transactional,
                                                       false))) {
        if (!starfield) {
            new_starfield->Destroy();
        }
        if (!orchestrator) {
            new_orchestrator->Destroy();
        }
        transaction.Cancel();
        return Result{std::unexpect, TEXT("Could not create ship post-processing.")};
    }

    world_settings->Modify();
    world_settings->DefaultGameMode = game_mode_class;
    new_orchestrator->Modify();
    new_orchestrator->set_start_mode(EOrchestratorStartMode::AuthoredLevel);
    new_orchestrator->set_presentation_enabled(true);
    new_orchestrator->set_level_script(s7_mission_path(document));
    new_orchestrator->clear_player_ship();
    new_orchestrator->set_level_config(*document.level_config);

    return FString::Printf(
        TEXT("Playable level ready: game mode, orchestrator, GPU starfield, post-processing, "
             "player control, and level script configured (%d entities) using %s. Save the map."),
        document.entities.Num(),
        *document.level_config->GetName());
}
}
