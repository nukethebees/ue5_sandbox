#include "SpaceGame/levels/LevelLoader.h"

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

void destroy_spawned_actors(TArray<AActor*> const& actors) {
    for (auto* const actor : actors) {
        if (IsValid(actor)) {
            actor->Destroy();
        }
    }
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

    TArray<AActor*> spawned_actors;
    auto const entities{definition.entities.get_const_view()};
    auto const entity_count{entities.num()};
    spawned_actors.Reserve(entity_count + (definition.camera.IsSet() ? 1 : 0));

    auto const player_index{has_player
                                ? definition.entities.ids.IndexOfByKey(definition.player_entity_id)
                                : INDEX_NONE};
    ATestSpaceShip* player{nullptr};
    if (has_player) {
        check(player_index != INDEX_NONE);
        auto const player_team{resolve_team(entities.teams[player_index])};
        check(player_team.IsSet());
        auto const player_transform{entity_transform(entities, player_index)};
        player = world->SpawnActorDeferred<ATestSpaceShip>(config->classes.player_ship_class,
                                                           player_transform);
        if (!IsValid(player)) {
            add_error(result,
                      ELevelLoadErrorCode::ActorSpawnFailed,
                      TEXT("Failed to begin spawning the player"));
            return result;
        }

        spawned_actors.Add(player);
        player->set_actor_config(&config->player_ship);
        player->set_team(player_team.GetValue());
        if (!finish_spawn(*player, player_transform)) {
            destroy_spawned_actors(spawned_actors);
            add_error(result,
                      ELevelLoadErrorCode::ActorSpawnFailed,
                      TEXT("Failed to finish spawning the player"));
            return result;
        }
    }

    for (int32 i{0}; i < entity_count; ++i) {
        if (i == player_index) {
            continue;
        }

        auto const archetype{entities.archetypes[i]};
        auto const team{resolve_team(entities.teams[i])};
        check(team.IsSet());
        auto const transform{entity_transform(entities, i)};
        AActor* spawned_actor{nullptr};

        if (archetype == level_archetypes::capital_ship) {
            auto* const capital{world->SpawnActorDeferred<ATestCapitalShipProxy>(
                config->classes.capital_ship_proxy_class, transform)};
            if (IsValid(capital)) {
                capital->set_actor_config(&config->capital_ships);
                capital->set_team(team.GetValue());
                spawned_actor = capital;
            }
        } else {
            check(archetype == level_archetypes::static_turret);
            auto* const turret{world->SpawnActorDeferred<ATestStaticTurretsProxy>(
                ATestStaticTurretsProxy::StaticClass(), transform)};
            if (IsValid(turret)) {
                turret->set_actor_config(&config->turrets);
                turret->set_team(team.GetValue());
                spawned_actor = turret;
            }
        }

        if (!IsValid(spawned_actor)) {
            destroy_spawned_actors(spawned_actors);
            add_error(result,
                      ELevelLoadErrorCode::ActorSpawnFailed,
                      FString::Printf(TEXT("Failed to begin spawning entity %s"),
                                      *entity_label(entities, i)));
            return result;
        }

        spawned_actors.Add(spawned_actor);
        if (!finish_spawn(*spawned_actor, transform)) {
            destroy_spawned_actors(spawned_actors);
            add_error(result,
                      ELevelLoadErrorCode::ActorSpawnFailed,
                      FString::Printf(TEXT("Failed to finish spawning entity %s"),
                                      *entity_label(entities, i)));
            return result;
        }
    }

    if (definition.camera.IsSet()) {
        auto const& camera_definition{definition.camera.GetValue()};
        FVector focus{FVector::ZeroVector};
        for (auto const target_id : camera_definition.target_entity_ids) {
            auto const target_index{definition.entities.ids.IndexOfByKey(target_id)};
            check(target_index != INDEX_NONE);
            focus += entities.positions[target_index];
        }
        focus /= camera_definition.target_entity_ids.Num();

        auto const camera_position{focus + camera_definition.offset_direction.GetSafeNormal() *
                                               camera_definition.distance};
        auto const camera_transform{
            FTransform{(focus - camera_position).Rotation(), camera_position}};
        auto* const camera{
            world->SpawnActorDeferred<ACameraActor>(ACameraActor::StaticClass(), camera_transform)};
        if (!IsValid(camera)) {
            destroy_spawned_actors(spawned_actors);
            add_error(result,
                      ELevelLoadErrorCode::ActorSpawnFailed,
                      TEXT("Failed to begin spawning the initial camera"));
            return result;
        }

        spawned_actors.Add(camera);
        if (!finish_spawn(*camera, camera_transform)) {
            destroy_spawned_actors(spawned_actors);
            add_error(result,
                      ELevelLoadErrorCode::ActorSpawnFailed,
                      TEXT("Failed to finish spawning the initial camera"));
            return result;
        }

        auto* const player_controller{UGameplayStatics::GetPlayerController(world, 0)};
        if (!IsValid(player_controller)) {
            destroy_spawned_actors(spawned_actors);
            add_error(result,
                      ELevelLoadErrorCode::MissingInfrastructure,
                      TEXT("Initial camera requires a local player controller"));
            return result;
        }
        player_controller->SetViewTarget(camera);
    } else {
        check(IsValid(player));
        orchestrator_.set_player_ship(*player);
    }
    return result;
}
}
