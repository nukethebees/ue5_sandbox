#include "SpaceGame/levels/LevelLoader.h"

#include "LevelEntityTableOperations.h"
#include "LevelTeamResolution.h"

#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <Camera/CameraActor.h>
#include <Engine/World.h>
#include <EngineUtils.h>
#include <GameFramework/PlayerController.h>
#include <Kismet/GameplayStatics.h>

namespace ml {
namespace {
void add_error(FLevelLoadResult& result, ELevelLoadErrorCode const code, FString message) {
    result.errors.Add(FLevelLoadError{.code = code, .message = MoveTemp(message)});
}

template <typename T>
auto world_contains(UWorld& world) -> bool {
    for (TActorIterator<T> it{&world}; it; ++it) {
        return true;
    }
    return false;
}

auto finish_spawn(AActor& actor, FTransform const& transform) -> bool {
    return IsValid(UGameplayStatics::FinishSpawningActor(&actor, transform));
}

class FSpawnedActorTransaction final {
  public:
    explicit FSpawnedActorTransaction(int32 const expected_actor_count) {
        actors_.Reserve(expected_actor_count);
    }

    FSpawnedActorTransaction(FSpawnedActorTransaction const&) = delete;
    auto operator=(FSpawnedActorTransaction const&) -> FSpawnedActorTransaction& = delete;

    ~FSpawnedActorTransaction() {
        if (committed_) {
            return;
        }
        for (auto* const actor : actors_) {
            if (IsValid(actor)) {
                actor->Destroy();
            }
        }
    }

    void add(AActor& actor) { actors_.Add(&actor); }
    void commit() noexcept { committed_ = true; }
  private:
    TArray<AActor*> actors_{};
    bool committed_{false};
};

auto spawn_player(UWorld& world,
                  USpaceGameLevelConfig const& config,
                  FLevelEntityTableConstView const& entities,
                  int32 const player_index,
                  FSpawnedActorTransaction& transaction,
                  FLevelLoadResult& result) -> ATestSpaceShip* {
    auto const entity{level_entity_table_detail::get(entities, player_index)};
    auto const team{level_team_detail::resolve(entity.team)};
    check(team.IsSet());
    auto const transform{FTransform{entity.rotation, entity.position}};
    auto* const player{
        world.SpawnActorDeferred<ATestSpaceShip>(config.classes.player_ship_class, transform)};
    if (!IsValid(player)) {
        add_error(result,
                  ELevelLoadErrorCode::ActorSpawnFailed,
                  TEXT("Failed to begin spawning the player"));
        return nullptr;
    }

    transaction.add(*player);
    player->set_actor_config(&config.player_ship);
    player->set_team(team.GetValue());
    if (!finish_spawn(*player, transform)) {
        add_error(result,
                  ELevelLoadErrorCode::ActorSpawnFailed,
                  TEXT("Failed to finish spawning the player"));
        return nullptr;
    }
    return player;
}

auto initial_camera_transform(FLevelDefinition const& definition,
                              FLevelEntityTableConstView const& entities) -> FTransform {
    auto const& camera{definition.camera.GetValue()};
    FVector focus{FVector::ZeroVector};
    for (auto const target_id : camera.target_entity_ids) {
        auto const target_index{level_entity_table_detail::find_index(entities, target_id)};
        check(target_index != INDEX_NONE);
        focus += level_entity_table_detail::get(entities, target_index).position;
    }
    focus /= camera.target_entity_ids.Num();

    auto const position{focus + camera.offset_direction.GetSafeNormal() * camera.distance};
    return FTransform{(focus - position).Rotation(), position};
}

auto spawn_initial_camera(UWorld& world,
                          APlayerController& player_controller,
                          FLevelDefinition const& definition,
                          FLevelEntityTableConstView const& entities,
                          FSpawnedActorTransaction& transaction,
                          FLevelLoadResult& result) -> bool {
    auto const transform{initial_camera_transform(definition, entities)};
    auto* const camera{
        world.SpawnActorDeferred<ACameraActor>(ACameraActor::StaticClass(), transform)};
    if (!IsValid(camera)) {
        add_error(result,
                  ELevelLoadErrorCode::ActorSpawnFailed,
                  TEXT("Failed to begin spawning the initial camera"));
        return false;
    }

    transaction.add(*camera);
    if (!finish_spawn(*camera, transform)) {
        add_error(result,
                  ELevelLoadErrorCode::ActorSpawnFailed,
                  TEXT("Failed to finish spawning the initial camera"));
        return false;
    }

    player_controller.SetViewTarget(camera);
    return true;
}

}

auto FLevelLoader::load(FLevelDefinition const& definition) const -> FLevelLoadResult {
    FLevelLoadResult result;
    auto validation{validate_level(definition)};
    if (!validation) {
        result.validation_errors = MoveTemp(validation.errors);
        add_error(result,
                  ELevelLoadErrorCode::InvalidDefinition,
                  TEXT("Level definition failed validation"));
        return result;
    }

    if (orchestrator_.get_state() != EOrchestratorState::Uninitialised) {
        add_error(result,
                  ELevelLoadErrorCode::InvalidOrchestratorState,
                  TEXT("Level loader requires an uninitialised orchestrator"));
        return result;
    }

    auto* const world{orchestrator_.GetWorld()};
    if (!IsValid(world)) {
        add_error(result, ELevelLoadErrorCode::InvalidWorld, TEXT("Orchestrator world is invalid"));
        return result;
    }

    auto const* const config{orchestrator_.get_level_config()};
    auto const has_player{definition.player_entity_id.is_set()};
    auto const presentation_enabled{orchestrator_.is_presentation_enabled()};
    if (has_player && !presentation_enabled) {
        add_error(result,
                  ELevelLoadErrorCode::InvalidDefinition,
                  TEXT("Presentation-disabled simulation requires a playerless level"));
        return result;
    }
    if (!IsValid(config) || !config->is_valid(presentation_enabled) ||
        (has_player && !IsValid(config->classes.player_ship_class))) {
        add_error(result,
                  ELevelLoadErrorCode::InvalidLevelConfig,
                  TEXT("Level configuration or required actor classes are invalid"));
        return result;
    }

    if (IsValid(orchestrator_.get_player_ship()) || world_contains<ATestSpaceShip>(*world) ||
        world_contains<ATestCapitalShipProxy>(*world) ||
        world_contains<ATestStaticTurretsProxy>(*world)) {
        add_error(result,
                  ELevelLoadErrorCode::ExistingLevelActors,
                  TEXT("World already contains authored gameplay actors"));
        return result;
    }

    auto* const player_controller{
        presentation_enabled ? UGameplayStatics::GetPlayerController(world, 0) : nullptr};
    if (presentation_enabled && !IsValid(player_controller)) {
        add_error(result,
                  ELevelLoadErrorCode::MissingInfrastructure,
                  TEXT("Authored level requires a local player controller"));
        return result;
    }

    orchestrator_.prepare_level();
    if (presentation_enabled && !orchestrator_.get_presentation_resources().is_valid()) {
        add_error(result,
                  ELevelLoadErrorCode::MissingInfrastructure,
                  TEXT("Orchestrator simulation infrastructure is incomplete"));
        return result;
    }

    auto const entities{definition.entities.get_const_view()};
    auto const entity_count{entities.num()};
    FSpawnedActorTransaction transaction{1 + (definition.camera.IsSet() ? 1 : 0)};

    auto const player_index{
        has_player ? level_entity_table_detail::find_index(entities, definition.player_entity_id)
                   : INDEX_NONE};
    ATestSpaceShip* player{nullptr};
    if (has_player) {
        check(player_index != INDEX_NONE);
        player = spawn_player(*world, *config, entities, player_index, transaction, result);
        if (!IsValid(player)) {
            return result;
        }
    }

    if (presentation_enabled && definition.camera.IsSet()) {
        if (!spawn_initial_camera(
                *world, *player_controller, definition, entities, transaction, result)) {
            return result;
        }
    } else if (presentation_enabled) {
        check(IsValid(player));
        orchestrator_.set_player_ship(*player);
        player_controller->Possess(player);
    }

    orchestrator_.set_level_definition(definition);
    transaction.commit();
    return result;
}
}
