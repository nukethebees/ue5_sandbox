#include <SpaceGame/simulation/LevelWorldReset.h>

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

namespace ioj {
void reset_level_world(UWorld& world, AActor const& retained_actor) {
    for (TActorIterator<AActor> it{&world}; it;) {
        auto* const actor{*it};
        ++it;
        auto const retained{actor == &retained_actor || actor_is_any<AWorldSettings,
                                                                     AGameModeBase,
                                                                     AGameStateBase,
                                                                     APlayerController,
                                                                     APlayerState,
                                                                     AHUD,
                                                                     ALevelScriptActor,
                                                                     APhysicsVolume>(*actor)};
        if (!retained) {
            actor->Destroy();
        }
    }
}
}
