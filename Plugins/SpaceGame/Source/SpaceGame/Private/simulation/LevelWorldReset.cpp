#include <SpaceGame/simulation/LevelWorldReset.h>

#include <SpaceGame/missions/LevelMissionDefinition.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCoreEngine/actor_utils.h>

#include <Engine/LevelScriptActor.h>
#include <Engine/World.h>
#include <EngineUtils.h>
#include <GameFramework/GameModeBase.h>
#include <GameFramework/GameStateBase.h>
#include <GameFramework/HUD.h>
#include <GameFramework/PhysicsVolume.h>
#include <GameFramework/PlayerController.h>
#include <GameFramework/PlayerState.h>
#include <GameFramework/WorldSettings.h>
#include <Kismet/GameplayStatics.h>

namespace ioj {
namespace {

auto recreate_player(UWorld& world,
                     TObjectPtr<ATestSpaceShip>& player,
                     FLevelMissionDefinition& mission) -> ATestSpaceShip* {
    if (!IsValid(player)) {
        return nullptr;
    }

    auto* const previous{player.Get()};
    auto* const actor_class{previous->GetClass()};
    if (!IsValid(actor_class)) {
        UE_LOG(LogSandbox, Fatal, TEXT("Cannot reset level world: player class is invalid"));
        return nullptr;
    }

    if (!previous->Destroy()) {
        UE_LOG(LogSandbox,
               Fatal,
               TEXT("Cannot reset level world: failed to destroy %s"),
               *previous->GetName());
        return nullptr;
    }

    auto* const replacement{
        world.SpawnActorDeferred<ATestSpaceShip>(actor_class, FTransform::Identity)};
    if (!IsValid(replacement)) {
        UE_LOG(LogSandbox,
               Fatal,
               TEXT("Cannot reset level world: failed to spawn %s"),
               *actor_class->GetName());
        player = nullptr;
        return nullptr;
    }

    mission.replace_startup_actor(previous, *replacement);
    player = replacement;
    return replacement;
}

auto is_retained_actor(AActor const& actor,
                       AActor const& retained_actor,
                       ATestSpaceShip const* const replacement) -> bool {
    return &actor == &retained_actor || &actor == replacement ||
           actor_is_any<AWorldSettings,
                        AGameModeBase,
                        AGameStateBase,
                        APlayerController,
                        APlayerState,
                        AHUD,
                        ALevelScriptActor,
                        APhysicsVolume>(actor);
}

}

void reset_level_world(UWorld& world,
                       AActor const& retained_actor,
                       TObjectPtr<ATestSpaceShip>& player,
                       FLevelMissionDefinition& mission,
                       FPlayerShipConfig const* const player_config) {
    auto* const replacement{recreate_player(world, player, mission)};

    for (TActorIterator<AActor> it{&world}; it;) {
        auto* const actor{*it};
        ++it;
        if (!is_retained_actor(*actor, retained_actor, replacement)) {
            actor->Destroy();
        }
    }

    if (IsValid(player) && player_config) {
#if WITH_EDITOR
        if (!world.IsGameWorld()) {
            player->Modify();
        }
#endif
        player->set_actor_config(player_config);
    }

    if (replacement) {
        UGameplayStatics::FinishSpawningActor(replacement, FTransform::Identity);
    }
}

}
