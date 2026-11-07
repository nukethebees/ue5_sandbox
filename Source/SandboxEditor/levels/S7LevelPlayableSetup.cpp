#include "SandboxEditor/levels/S7LevelPlayableSetup.h"

#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringSession.h"
#include <ioj/levels/launch_options.h>
#include <Sandbox/environment/effects/ShipPostProcessing.h>
#include <SpaceGame/s7/GameSettingsReader.h>
#include <SpaceGame/simulation/LevelSimulationBuilder.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/system/GameSubsystem.h>

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

auto launch_s7_level_for_pie(UWorld& world,
                             levels::LevelDefinition definition,
                             sim::LevelSimInitData data) -> std::expected<void, FString> {
    AS7LevelAuthoringDocument* document{};
    ATestBatchOrchestrator* orchestrator{};
    for (TActorIterator<AS7LevelAuthoringDocument> it{&world}; it; ++it) {
        document = *it;
    }
    for (TActorIterator<ATestBatchOrchestrator> it{&world}; it; ++it) {
        orchestrator = *it;
    }
    check(document && orchestrator);
    auto* const game{UGameSubsystem::get(world.GetGameInstance())};
    if (!IsValid(game)) {
        return std::unexpected{TEXT("The PIE game subsystem is unavailable.")};
    }
    levels::LaunchOptions options;
    options.control_context =
        definition.camera() ? levels::ControlContext::Observer : levels::ControlContext::Player;
    orchestrator->clear_player_ship();
    for (auto const& binding : document->entities) {
        if (IsValid(binding.actor) && !world.DestroyActor(binding.actor.Get(), false, false)) {
            return std::unexpected{TEXT("Could not remove an authoring preview actor before PIE.")};
        }
    }
    orchestrator->set_initialisation_data(std::move(data));
    game->set_pending_level(std::move(definition), document->source_path, options);
    return {};
}

auto validate_playable_s7_level(ULevel& level, AS7LevelAuthoringDocument& document)
    -> std::expected<FString, FString> {
    auto exported{export_s7_editor_config(level, document)};
    if (!exported) {
        return std::unexpected{exported.error()};
    }
    auto definition{read_s7_authored_level(document)};
    if (!definition) {
        return std::unexpected{definition.error()};
    }
    auto compiled{compile_playable_s7_definition(*definition, *document.level_config)};
    if (!compiled) {
        return std::unexpected{compiled.error()};
    }
    return TEXT("The composed Scheme level passed compilation and initialization validation.");
}

auto compile_playable_s7_definition(levels::LevelDefinition const& definition,
                                    USpaceGameLevelConfig const& resources)
    -> std::expected<sim::LevelSimInitData, FString> {
    auto prepared{prepare_level_resources(definition.settings(), resources, GetTransientPackage())};
    if (!prepared) {
        return std::unexpected{std::move(prepared.error())};
    }
    TStrongObjectPtr<USpaceGameLevelConfig> effective_config{*prepared};
    levels::LaunchOptions const options;
    auto build{prepare_level_simulation({.config = *effective_config,
                                         .clock_settings = definition.settings().tick,
                                         .launch_options = options,
                                         .definition = definition})};
    if (!build) {
        return std::unexpected{build.error().format()};
    }
    return std::move(*build);
}

auto set_up_playable_s7_level(ULevel& level, AS7LevelAuthoringDocument& document)
    -> std::expected<FString, FString> {
    auto* const world{level.OwningWorld.Get()};
    auto* const world_settings{IsValid(world) ? world->GetWorldSettings() : nullptr};
    if (!GEditor || !IsValid(world) || world->GetCurrentLevel() != &level ||
        !IsValid(world_settings) || document.GetLevel() != &level) {
        return std::unexpected{TEXT("The current editor level is unavailable.")};
    }
    auto const validation{validate_playable_s7_level(level, document)};
    if (!validation) {
        return std::unexpected{validation.error()};
    }
    auto* const game_mode_class{
        LoadClass<AGameModeBase>(nullptr, s7_level_playable_setup_detail::game_mode_class_path)};
    if (!IsValid(game_mode_class)) {
        return std::unexpected{TEXT("The Space Ship game mode asset is unavailable.")};
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
                return std::unexpected{TEXT("The level contains multiple batch orchestrators.")};
            }
            orchestrator = candidate;
        }
        if (auto* const candidate{Cast<AGpuStarfieldActor>(actor)}) {
            if (starfield) {
                return std::unexpected{TEXT("The level contains multiple GPU starfields.")};
            }
            starfield = candidate;
        }
        if (auto* const candidate{Cast<AShipPostProcessing>(actor)}) {
            if (post_processing) {
                return std::unexpected{
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
            return std::unexpected{TEXT("Could not create the batch orchestrator.")};
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
        return std::unexpected{TEXT("Could not create the GPU starfield.")};
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
        return std::unexpected{TEXT("Could not create ship post-processing.")};
    }

    world_settings->Modify();
    world_settings->DefaultGameMode = game_mode_class;
    new_orchestrator->Modify();
    new_orchestrator->set_start_mode(EOrchestratorStartMode::AuthoredLevel);
    new_orchestrator->set_presentation_enabled(true);
    new_orchestrator->set_level_script(document.source_path);
    new_orchestrator->clear_player_ship();
    new_orchestrator->set_level_config(*document.level_config);

    return FString::Printf(
        TEXT("Playable level ready: game mode, orchestrator, GPU starfield, post-processing, "
             "player control, and mission configured (%d entities) using %s. Save the map."),
        document.entities.Num(),
        *document.level_config->GetName());
}
}
