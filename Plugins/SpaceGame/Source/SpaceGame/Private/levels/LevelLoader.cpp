#include "SpaceGame/levels/LevelLoader.h"

#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/UnrealMessages.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCoreEngine/strings.h>
#include <SandboxCoreEngine/TransformConversion.h>
#include <SandboxCoreEngine/VectorConversion.h>

#include <Camera/CameraActor.h>
#include <CoreGlobals.h>
#include <Engine/World.h>
#include <EngineUtils.h>
#include <GameFramework/PlayerController.h>
#include <Kismet/GameplayStatics.h>

namespace ioj {
namespace {
void add_error(FLevelLoadErrors& result, ELevelLoadErrorCode const code, FString message) {
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
        for (auto* const actor : actors_) {
            if (IsValid(actor)) {
                actor->Destroy();
            }
        }
    }

    void add(AActor& actor) { actors_.Add(&actor); }
    void commit() noexcept { actors_.Reset(); }
  private:
    TArray<AActor*> actors_{};
};

auto spawn_player(UWorld& world,
                  USpaceGameLevelConfig const& config,
                  presentation::PlayerSpawn const& entity,
                  FSpawnedActorTransaction& transaction,
                  FLevelLoadErrors& result) -> ATestSpaceShip* {
    auto const team{to_unreal(entity.team)};
    auto const transform{
        FTransform{FRotator{entity.rotation.pitch, entity.rotation.yaw, entity.rotation.roll},
                   ioj::to_unreal(entity.position)}};
    auto* const player{
        world.SpawnActorDeferred<ATestSpaceShip>(config.classes.player_ship_class, transform)};
    if (!IsValid(player)) {
        add_error(result,
                  ELevelLoadErrorCode::ActorSpawnFailed,
                  TEXT("Failed to begin spawning the player"));
        return nullptr;
    }

    transaction.add(*player);
    player->set_team(team);
    if (entity.initial_health) {
        player->set_initial_health(*entity.initial_health);
    }

    if (!finish_spawn(*player, transform)) {
        add_error(result,
                  ELevelLoadErrorCode::ActorSpawnFailed,
                  TEXT("Failed to finish spawning the player"));
        return nullptr;
    }

    return player;
}

auto spawn_initial_camera(UWorld& world,
                          ASpaceGamePlayerController& player_controller,
                          Transform3d const& initial_camera,
                          FSpawnedActorTransaction& transaction,
                          FLevelLoadErrors& result,
                          levels::ControlContext const control_context) -> bool {
    auto const transform{to_unreal(initial_camera)};
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

    if (!player_controller.activate_playerless_camera(*camera, control_context)) {
        add_error(result,
                  ELevelLoadErrorCode::MissingInfrastructure,
                  TEXT("Failed to activate the playerless control context"));
        return false;
    }

    return true;
}

}

auto FLevelLoadErrors::format(FStringView const source_path) const -> FString {
    TArray<FString> messages;
    messages.Reserve(errors.Num());
    for (auto const& error : errors) {
        messages.Add(error.message);
    }

    return FString::Printf(TEXT("Failed to load '%s':\n%s"),
                           *FString{source_path},
                           *FString::Join(messages, TEXT("\n")));
}

void FLevelLoader::report_failure(UWorld* const world, FString message) {
    UE_LOG(LogSandbox, Error, TEXT("%s"), *message);
    request_menu({.launch_error = to_string(message)});
}

auto FLevelLoader::load(FLevelActorLoadRequest const& request) -> FLevelLoadResult {
    FLevelLoadResult result{std::unexpect};
    auto& errors{result.error()};
    auto& world{request.world};
    auto const& mission{request.mission};
    auto const& config{request.config};
    auto const has_player{mission.player.has_value()};
    auto const& visual{request.visual};
    if (!has_player && visual &&
        visual->playerless_control_context != levels::ControlContext::Observer &&
        visual->playerless_control_context != levels::ControlContext::Benchmark) {
        add_error(errors,
                  ELevelLoadErrorCode::InvalidDefinition,
                  TEXT("Playerless level requires Observer or Benchmark control context"));
        return result;
    }

    if (has_player && visual && !IsValid(config.classes.player_ship_class)) {
        add_error(errors,
                  ELevelLoadErrorCode::InvalidLevelConfig,
                  TEXT("Level configuration or required actor classes are invalid"));
        return result;
    }

    if (IsValid(request.player) || world_contains<ATestSpaceShip>(world)) {
        add_error(errors,
                  ELevelLoadErrorCode::ExistingLevelActors,
                  TEXT("World already contains authored gameplay actors"));
        return result;
    }

    FSpawnedActorTransaction transaction{1 + (mission.camera ? 1 : 0)};
    ATestSpaceShip* player{nullptr};
    if (has_player && visual) {
        player = spawn_player(world, config, *mission.player, transaction, errors);
        if (!IsValid(player)) {
            return result;
        }
    }

    if (visual && mission.camera) {
        if (!spawn_initial_camera(world,
                                  visual->controller,
                                  *mission.camera,
                                  transaction,
                                  errors,
                                  visual->playerless_control_context)) {
            return result;
        }
    } else if (visual) {
        check(IsValid(player));
        request.player = player;
        visual->controller.Possess(player);
    }

    transaction.commit();
    return {};
}
}
