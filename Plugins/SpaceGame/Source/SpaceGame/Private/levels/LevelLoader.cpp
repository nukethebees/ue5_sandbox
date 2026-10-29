#include "SpaceGame/levels/LevelLoader.h"

#include "SpaceGame/levels/ObserverCameraTransform.h"
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

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
                  levels::EntitySpawnDefinition const& entity,
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
    player->set_actor_config(&config.player_ship);
    player->set_team(team);
    if (entity.overrides.health) {
        player->set_initial_health(*entity.overrides.health);
    }
    if (!finish_spawn(*player, transform)) {
        add_error(result,
                  ELevelLoadErrorCode::ActorSpawnFailed,
                  TEXT("Failed to finish spawning the player"));
        return nullptr;
    }
    return player;
}

auto initial_camera_transform(levels::LevelDefinition const& definition) -> FTransform {
    auto const& camera{*definition.camera()};
    TArray<FVector> target_positions;
    target_positions.Reserve(static_cast<int32>(camera.target_entity_ids.size()));
    for (auto const& target_id : camera.target_entity_ids) {
        auto const position{definition.entity(target_id).position};
        target_positions.Add(ioj::to_unreal(position));
    }
    auto const transform{make_observer_camera_transform(
        target_positions, ioj::to_unreal(camera.offset_direction), camera.distance)};
    check(transform.IsSet());
    return transform->transform;
}

auto spawn_initial_camera(UWorld& world,
                          ASpaceGamePlayerController& player_controller,
                          levels::LevelDefinition const& definition,
                          FSpawnedActorTransaction& transaction,
                          FLevelLoadErrors& result,
                          EPlayerControlContext const control_context) -> bool {
    auto const transform{initial_camera_transform(definition)};
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

auto FLevelLoader::take_pending(UGameInstance* const game_instance)
    -> std::expected<FPendingLevelDefinition, FString> {
    auto* const subsystem{UGameSubsystem::get(game_instance)};
    if (!IsValid(subsystem)) {
        return std::unexpected{
            FString{TEXT("Cannot load authored level: game subsystem is invalid")}};
    }
    auto pending{subsystem->take_pending_level()};
    if (!pending.IsSet()) {
        return std::unexpected{
            FString{TEXT("No pending authored level was provided to GameRuntime.")}};
    }

    auto const& options{pending->options};
    if (!level_launch::is_valid_time_scale(options.requested_time_scale)) {
        return std::unexpected{
            FString::Printf(TEXT("Cannot load authored level: requested time scale %.17g is "
                                 "outside the supported range (0, %.0f]."),
                            options.requested_time_scale,
                            level_launch::maximum_time_scale)};
    }
    if (options.presentation_mode == ELevelPresentationMode::SimulationOnly &&
        !options.simulated_duration_seconds.IsSet()) {
        return std::unexpected{
            FString{TEXT("Cannot load authored level: simulation-only runs require a duration.")}};
    }
    if (options.simulated_duration_seconds.IsSet() &&
        (!FMath::IsFinite(options.simulated_duration_seconds.GetValue()) ||
         options.simulated_duration_seconds.GetValue() <= 0.0)) {
        return std::unexpected{FString{
            TEXT("Cannot load authored level: simulated duration must be finite and positive.")}};
    }
    return MoveTemp(pending.GetValue());
}

void FLevelLoader::report_failure(UWorld* const world, FString message) {
    UE_LOG(LogSandbox, Error, TEXT("%s"), *message);
#if WITH_EDITOR
    if (IsValid(world) && world->WorldType == EWorldType::PIE) {
        return;
    }
#endif
    if (GIsAutomationTesting) {
        return;
    }

    auto* const game_instance{IsValid(world) ? world->GetGameInstance() : nullptr};
    auto* const subsystem{UGameSubsystem::get(game_instance)};
    if (IsValid(subsystem)) {
        subsystem->set_level_launch_error(MoveTemp(message));
        if (subsystem->return_to_level_select()) {
            return;
        }
    }
    if (IsValid(world)) {
        UGameplayStatics::OpenLevel(world, UGameSubsystem::get_main_menu_level_name());
    }
}

auto FLevelLoader::load(FLevelActorLoadRequest const& request)
    -> std::expected<void, FLevelLoadErrors> {
    std::expected<void, FLevelLoadErrors> result{std::unexpect};
    auto& errors{result.error()};
    auto& world{request.world};
    auto const& definition{request.definition};
    auto const& config{request.config};
    auto const has_player{!definition.player_entity_id().empty()};
    auto const& visual{request.visual};
    if (!has_player && visual &&
        visual->playerless_control_context != EPlayerControlContext::Observer &&
        visual->playerless_control_context != EPlayerControlContext::Benchmark) {
        add_error(errors,
                  ELevelLoadErrorCode::InvalidDefinition,
                  TEXT("Playerless level requires Observer or Benchmark control context"));
        return result;
    }
    if (has_player && !visual) {
        add_error(errors,
                  ELevelLoadErrorCode::InvalidDefinition,
                  TEXT("Presentation-disabled simulation requires a playerless level"));
        return result;
    }
    if (has_player && !IsValid(config.classes.player_ship_class)) {
        add_error(errors,
                  ELevelLoadErrorCode::InvalidLevelConfig,
                  TEXT("Level configuration or required actor classes are invalid"));
        return result;
    }

    if (IsValid(request.player) || world_contains<ATestSpaceShip>(world) ||
        world_contains<ATestCapitalShipProxy>(world) ||
        world_contains<ATestStaticTurretsProxy>(world)) {
        add_error(errors,
                  ELevelLoadErrorCode::ExistingLevelActors,
                  TEXT("World already contains authored gameplay actors"));
        return result;
    }

    FSpawnedActorTransaction transaction{1 + (definition.camera() ? 1 : 0)};
    ATestSpaceShip* player{nullptr};
    if (has_player) {
        player = spawn_player(
            world, config, definition.entity(definition.player_entity_id()), transaction, errors);
        if (!IsValid(player)) {
            return result;
        }
    }

    if (visual && definition.camera()) {
        if (!spawn_initial_camera(world,
                                  visual->controller,
                                  definition,
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
