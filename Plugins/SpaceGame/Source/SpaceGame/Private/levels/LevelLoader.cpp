#include "SpaceGame/levels/LevelLoader.h"

#include "LevelArchetypeResolution.h"

#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/effects/DelayedNiagaraSpawner.h>
#include <SpaceGame/entities/TestTeam.h>
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
auto resolve_team(FLevelTeamId const id) -> TOptional<ETestTeam> {
    if (id == level_teams::white) {
        return ETestTeam::White;
    }
    if (id == level_teams::red) {
        return ETestTeam::Red;
    }
    if (id == level_teams::green) {
        return ETestTeam::Green;
    }
    if (id == level_teams::blue) {
        return ETestTeam::Blue;
    }
    if (id == level_teams::orange) {
        return ETestTeam::Orange;
    }
    if (id == level_teams::yellow) {
        return ETestTeam::Yellow;
    }
    return NullOpt;
}

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

auto entity_transform(FLevelEntityTableConstView const& entities, int32 const index) -> FTransform {
    return FTransform{FRotator{entities.rotations.pitches[index],
                               entities.rotations.yaws[index],
                               entities.rotations.rolls[index]},
                      entities.positions[index]};
}

auto entity_label(FLevelEntityTableConstView const& entities, int32 const index) -> FString {
    auto const id{entities.ids[index]};
    return id.is_set() ? FString::Printf(TEXT("'%s'"), *id.value.ToString())
                       : FString::Printf(TEXT("at row %d"), index);
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
    auto const team{resolve_team(entities.teams[player_index])};
    check(team.IsSet());
    auto const transform{entity_transform(entities, player_index)};
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

auto spawn_entity(UWorld& world,
                  USpaceGameLevelConfig const& config,
                  FLevelEntityTableConstView const& entities,
                  int32 const index,
                  FSpawnedActorTransaction& transaction,
                  FLevelLoadResult& result) -> bool {
    auto const team{resolve_team(entities.teams[index])};
    auto const archetype{level_archetype_detail::resolve(entities.archetypes[index])};
    check(team.IsSet());
    check(archetype.IsSet());

    auto const transform{entity_transform(entities, index)};
    AActor* spawned_actor{nullptr};
    switch (archetype.GetValue()) {
        case level_archetype_detail::EResolvedArchetype::CapitalShip: {
            auto* const capital{world.SpawnActorDeferred<ATestCapitalShipProxy>(
                config.classes.capital_ship_proxy_class, transform)};
            if (IsValid(capital)) {
                capital->set_actor_config(&config.capital_ships);
                capital->set_team(team.GetValue());
                spawned_actor = capital;
            }
            break;
        }
        case level_archetype_detail::EResolvedArchetype::StaticTurret: {
            auto* const turret{world.SpawnActorDeferred<ATestStaticTurretsProxy>(
                ATestStaticTurretsProxy::StaticClass(), transform)};
            if (IsValid(turret)) {
                turret->set_actor_config(&config.turrets);
                turret->set_team(team.GetValue());
                spawned_actor = turret;
            }
            break;
        }
        case level_archetype_detail::EResolvedArchetype::PlayerFighter:
            checkNoEntry();
            return false;
    }

    if (!IsValid(spawned_actor)) {
        add_error(result,
                  ELevelLoadErrorCode::ActorSpawnFailed,
                  FString::Printf(TEXT("Failed to begin spawning entity %s"),
                                  *entity_label(entities, index)));
        return false;
    }

    transaction.add(*spawned_actor);
    if (!finish_spawn(*spawned_actor, transform)) {
        add_error(result,
                  ELevelLoadErrorCode::ActorSpawnFailed,
                  FString::Printf(TEXT("Failed to finish spawning entity %s"),
                                  *entity_label(entities, index)));
        return false;
    }
    return true;
}

auto initial_camera_transform(FLevelDefinition const& definition,
                              FLevelEntityTableConstView const& entities) -> FTransform {
    auto const& camera{definition.camera.GetValue()};
    FVector focus{FVector::ZeroVector};
    for (auto const target_id : camera.target_entity_ids) {
        auto const target_index{definition.entities.ids.IndexOfByKey(target_id)};
        check(target_index != INDEX_NONE);
        focus += entities.positions[target_index];
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
    if (!IsValid(config) || !config->is_valid() ||
        (has_player && !IsValid(config->classes.player_ship_class)) ||
        !IsValid(config->classes.capital_ship_proxy_class)) {
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
        definition.camera.IsSet() ? UGameplayStatics::GetPlayerController(world, 0) : nullptr};
    if (definition.camera.IsSet() && !IsValid(player_controller)) {
        add_error(result,
                  ELevelLoadErrorCode::MissingInfrastructure,
                  TEXT("Initial camera requires a local player controller"));
        return result;
    }

    orchestrator_.spawn_missing_actors();
    if (!IsValid(orchestrator_.get_lasers()) || !IsValid(orchestrator_.get_capital_ships()) ||
        !IsValid(orchestrator_.get_capital_ship_fighters()) ||
        !IsValid(orchestrator_.get_turrets()) || !IsValid(orchestrator_.get_spinners()) ||
        !IsValid(orchestrator_.get_niagara_spawner())) {
        add_error(result,
                  ELevelLoadErrorCode::MissingInfrastructure,
                  TEXT("Orchestrator simulation infrastructure is incomplete"));
        return result;
    }

    auto const entities{definition.entities.get_const_view()};
    auto const entity_count{entities.num()};
    FSpawnedActorTransaction transaction{entity_count + (definition.camera.IsSet() ? 1 : 0)};

    auto const player_index{has_player
                                ? definition.entities.ids.IndexOfByKey(definition.player_entity_id)
                                : INDEX_NONE};
    ATestSpaceShip* player{nullptr};
    if (has_player) {
        check(player_index != INDEX_NONE);
        player = spawn_player(*world, *config, entities, player_index, transaction, result);
        if (!IsValid(player)) {
            return result;
        }
    }

    for (int32 i{0}; i < entity_count; ++i) {
        if (i == player_index) {
            continue;
        }

        if (!spawn_entity(*world, *config, entities, i, transaction, result)) {
            return result;
        }
    }

    if (definition.camera.IsSet()) {
        check(IsValid(player_controller));
        if (!spawn_initial_camera(
                *world, *player_controller, definition, entities, transaction, result)) {
            return result;
        }
    } else {
        check(IsValid(player));
        orchestrator_.set_player_ship(*player);
    }

    transaction.commit();
    return result;
}
}
