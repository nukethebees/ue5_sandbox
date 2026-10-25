#include "SandboxEditor/levels/S7LevelPlayableSetup.h"

#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringSession.h"
#include <Sandbox/environment/effects/ShipPostProcessing.h>
#include <SpaceGame/levels/native_level_definition_conversion.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/LevelSimulationBuilder.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/system/GameSubsystem.h>

#include <SandboxCoreEngine/strings.h>
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
// Resolve the optional player only after collecting and validating the scene.
auto find_player(AS7LevelAuthoringDocument const& document, levels::EntityId const& id)
    -> ATestSpaceShip* {
    if (id.empty()) {
        return nullptr;
    }
    auto const* const binding{document.entities.FindByPredicate(
        [name = FName{to_fstring(id.value)}](auto const& candidate) {
            return candidate.id == name;
        })};
    check(binding);
    return CastChecked<ATestSpaceShip>(binding->actor.Get());
}

}

auto prepare_s7_level_for_pie(UWorld& world) -> std::expected<void, FString> {
    if (world.WorldType != EWorldType::PIE) {
        return {};
    }
    AS7LevelAuthoringDocument* document{};
    ATestBatchOrchestrator* orchestrator{};
    int32 documents{}, orchestrators{};
    for (auto* const level : world.GetLevels()) {
        for (auto const actor : level->Actors) {
            if (auto* const found{Cast<AS7LevelAuthoringDocument>(actor.Get())}; IsValid(found)) {
                document = found;
                ++documents;
            }
            if (auto* const found{Cast<ATestBatchOrchestrator>(actor.Get())}; IsValid(found)) {
                orchestrator = found;
                ++orchestrators;
            }
        }
    }
    if (documents == 0 || orchestrators == 0 ||
        (orchestrators == 1 &&
         orchestrator->get_start_mode() != EOrchestratorStartMode::AuthoredLevel &&
         orchestrator->get_start_mode() != EOrchestratorStartMode::Automatic)) {
        return {};
    }
    // Keep failed authoring launches paused instead of entering the proxy or missing-pending path.
    auto fail{[&](FString message) -> std::expected<void, FString> {
        for (TActorIterator<ATestBatchOrchestrator> it{&world}; it; ++it) {
            it->set_start_mode(EOrchestratorStartMode::Paused);
        }
        return std::unexpected{MoveTemp(message)};
    }};
    if (documents != 1 || orchestrators != 1) {
        return fail(TEXT("PIE authoring requires one document and one orchestrator."));
    }
    auto definition{collect_s7_editor_level(*document->GetLevel(), *document)};
    if (!definition) {
        return fail(definition.error());
    }
    auto* const game{UGameSubsystem::get(world.GetGameInstance())};
    if (!IsValid(game)) {
        return fail(TEXT("The PIE game subsystem is unavailable."));
    }

    FLevelLaunchOptions options;
    options.control_context =
        definition->camera() ? EPlayerControlContext::Observer : EPlayerControlContext::Player;
    // Upgrade old authoring maps in the PIE copy without changing their saved actors.
    orchestrator->set_start_mode(EOrchestratorStartMode::AuthoredLevel);
    orchestrator->clear_player_ship();
    for (auto const& binding : document->entities) {
        if (!world.DestroyActor(binding.actor.Get(), false, false)) {
            return fail(TEXT("Could not remove a duplicated authoring actor before PIE."));
        }
    }
    game->set_pending_level(*definition, TEXT("PIE scene"), options);
    return {};
}

auto validate_playable_s7_level(ULevel& level, AS7LevelAuthoringDocument& document)
    -> std::expected<FString, FString> {
    auto* const world{level.OwningWorld.Get()};
    auto* const config{document.level_config.Get()};
    if (!IsValid(world) || document.GetLevel() != &level) {
        return std::unexpected{TEXT("The current editor level is unavailable.")};
    }
    if (!IsValid(config)) {
        return std::unexpected{
            TEXT("The canonical S7 runtime level configuration is unavailable.")};
    }
    if (!config->is_valid(true)) {
        return std::unexpected{TEXT("The base S7 level configuration is invalid.")};
    }
    auto const definition{collect_s7_editor_level(level, document)};
    if (!definition) {
        return std::unexpected{definition.error()};
    }
    auto* const player{
        s7_level_playable_setup_detail::find_player(document, definition->player_entity_id())};

    TOptional<sim::player::PlayerSpawnData> player_spawn;
    UStaticMesh const* player_mesh{};
    if (player) {
        player_spawn.Emplace(player->make_spawn_data());
        player_mesh = player->get_collision_mesh();
    }
    TStrongObjectPtr<USpaceGameLevelConfig> effective_config{
        DuplicateObject<USpaceGameLevelConfig>(config, GetTransientPackage())};
    if (!effective_config.IsValid()) {
        return std::unexpected{TEXT("Could not prepare the authored collision-grid settings.")};
    }
    effective_config->collision_grid = document.resolve_collision_grid(config->collision_grid);
    if (!effective_config->is_valid(true)) {
        return std::unexpected{TEXT("The authored collision-grid settings are invalid.")};
    }
    auto const build{make_level_simulation_init_data(
        *effective_config, {}, *definition, MoveTemp(player_spawn), {}, player_mesh)};
    if (!build) {
        return std::unexpected{build.error().format()};
    }
    return FString::Printf(
        TEXT("Entity, fighter spawn, and collision grid checks passed using %s."),
        *config->GetName());
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
    new_orchestrator->clear_player_ship();
    new_orchestrator->set_level_config(*document.level_config);

    return FString::Printf(
        TEXT("Playable level ready: game mode, orchestrator, GPU starfield, post-processing, "
             "player control, and mission configured (%d entities) using %s. Save the map."),
        document.entities.Num(),
        *document.level_config->GetName());
}
}
