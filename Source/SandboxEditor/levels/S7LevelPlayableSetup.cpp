#include "SandboxEditor/levels/S7LevelPlayableSetup.h"

#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringSession.h"
#include <Sandbox/environment/effects/ShipPostProcessing.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/LevelSimulationBuilder.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxShaders/GpuStarfield/GpuStarfieldActor.h>

#include <Editor.h>
#include <Engine/Level.h>
#include <GameFramework/GameModeBase.h>
#include <GameFramework/WorldSettings.h>
#include <ScopedTransaction.h>
#include <UObject/StrongObjectPtr.h>

namespace ml::editor {
namespace s7_level_playable_setup_detail {
inline constexpr TCHAR game_mode_class_path[]{
    TEXT("/Game/GameModes/BP_SpaceShipGameMode.BP_SpaceShipGameMode_C")};
auto find_player(AS7LevelAuthoringDocument const& document)
    -> std::expected<ATestSpaceShip*, FString> {
    ATestSpaceShip* player{};
    for (auto const& binding : document.entities) {
        auto* const candidate{Cast<ATestSpaceShip>(binding.actor.Get())};
        if (!IsValid(candidate)) {
            continue;
        }
        if (player) {
            return std::unexpected{TEXT("The level contains more than one adopted player ship.")};
        }
        player = candidate;
    }
    if (!player && !document.use_observer_camera) {
        return std::unexpected{
            TEXT("Load and apply a script with a player ship before setting up this level.")};
    }
    return player;
}

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
    auto const player{s7_level_playable_setup_detail::find_player(document)};
    if (!player) {
        return std::unexpected{player.error()};
    }

    ::FLevelMissionDefinition mission;
    mission.mission_mode = document.mission.mode;
    mission.target_time = document.mission.time_limit_seconds;
    mission.kill_target =
        document.mission.use_explicit_kill_count ? document.mission.kill_count : 0;
    mission.startup_data.hero_entities = document.mission.heroes;
    mission.startup_data.entities_must_survive = document.mission.must_survive;
    mission.startup_data.entities_required_to_kill = document.mission.required_kills;
    mission.set_level_identity(document.level_id, document.title);

    TOptional<::ioj::sim::player::PlayerSpawnData> player_spawn;
    UStaticMesh const* player_mesh{};
    if (*player) {
        player_spawn.Emplace((*player)->make_spawn_data());
        player_mesh = (*player)->get_collision_mesh();
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
    auto const build{make_proxy_level_simulation_init_data(
        *effective_config, {}, *world, mission, MoveTemp(player_spawn), *player, player_mesh)};
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
    auto const player{s7_level_playable_setup_detail::find_player(document)};
    if (!player) {
        return std::unexpected{player.error()};
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
    new_orchestrator->set_start_mode(EOrchestratorStartMode::Automatic);
    new_orchestrator->set_presentation_enabled(true);
    if (*player) {
        (*player)->Modify();
        (*player)->AutoPossessPlayer = EAutoReceiveInput::Player0;
        new_orchestrator->set_player_ship(**player);
    } else {
        new_orchestrator->clear_player_ship();
    }
    for (auto const& binding : document.entities) {
        binding.actor->Modify();
    }
    new_orchestrator->set_level_config(*document.level_config);
    auto const grid{document.collision_grid_overrides()};
    new_orchestrator->set_collision_grid_override(
        grid.level_size.IsSet() ? grid.level_size.GetValue() : FVector3f::ZeroVector,
        grid.cell_size.IsSet() ? grid.cell_size.GetValue() : FVector3f::ZeroVector);

    auto& mission{new_orchestrator->get_mission_definition()};
    mission = {};
    mission.mission_mode = document.mission.mode;
    mission.target_time = document.mission.time_limit_seconds;
    mission.kill_target =
        document.mission.use_explicit_kill_count ? document.mission.kill_count : 0;
    mission.startup_data.hero_entities = document.mission.heroes;
    mission.startup_data.entities_must_survive = document.mission.must_survive;
    mission.startup_data.entities_required_to_kill = document.mission.required_kills;
    mission.set_level_identity(document.level_id, document.title);

    return FString::Printf(
        TEXT("Playable level ready: game mode, orchestrator, GPU starfield, post-processing, "
             "player control, and mission configured (%d entities) using %s. Save the map."),
        document.entities.Num(),
        *document.level_config->GetName());
}
}
