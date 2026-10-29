#include <SpaceGame/missions/LevelMissionDefinition.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/LevelWorldReset.h>
#include <SpaceGamePresentation/presentation/LevelActorSettings.h>

#include <SandboxCoreEngine/actor_utils.h>

#include <CQTest.h>
#include <Engine/World.h>
#include <GameFramework/PhysicsVolume.h>

TEST_CLASS(LevelWorldReset, "Sandbox.UnitTests")
{
    BEFORE_EACH()
    { world_ = UWorld::CreateWorld(EWorldType::Editor, false); }

    AFTER_EACH()
    {
        world_->DestroyWorld(false);
        world_ = nullptr;
    }

    TEST_METHOD(PlayerlessResetRetainsOwnerAndFrameworkActorsOnly)
    {
        auto* const retained{world_->SpawnActor<AActor>()};
        auto* const volume{world_->SpawnActor<APhysicsVolume>()};
        auto* const transient{world_->SpawnActor<AActor>()};
        if (!TestRunner->TestNotNull(TEXT("Retained actor exists"), retained) ||
            !TestRunner->TestNotNull(TEXT("Framework actor exists"), volume) ||
            !TestRunner->TestNotNull(TEXT("Transient actor exists"), transient)) {
            return;
        }
        TObjectPtr<ATestSpaceShip> player;
        FLevelMissionDefinition mission;

        ioj::reset_level_world(*world_, *retained, player, mission, nullptr);

        TestRunner->TestTrue(TEXT("Retained actor remains in the world"),
                             ioj::is_actor_in_world(*world_, retained));
        TestRunner->TestTrue(TEXT("Framework actor remains in the world"),
                             ioj::is_actor_in_world(*world_, volume));
        TestRunner->TestFalse(TEXT("Transient actor is removed"),
                              ioj::is_actor_in_world(*world_, transient));
        TestRunner->TestTrue(TEXT("Playerless levels remain playerless"), player == nullptr);
    }

    TEST_METHOD(RecreatesPlayerAndRebindsEveryMissionReference)
    {
        auto* const retained{world_->SpawnActor<AActor>()};
        TObjectPtr<ATestSpaceShip> player{world_->SpawnActor<ATestSpaceShip>()};
        if (!TestRunner->TestNotNull(TEXT("Retained actor exists"), retained) ||
            !TestRunner->TestNotNull(TEXT("Player exists"), player.Get())) {
            return;
        }
        player->SetActorLocation(FVector{500., 1000., 1500.});
        auto* const previous{player.Get()};
        auto* const previous_class{previous->GetClass()};
        FLevelMissionDefinition mission;
        mission.entity_bindings.Add(TEXT("player"), previous);
        mission.entity_bindings.Add(TEXT("protected-player"), previous);
        mission.entity_bindings.Add(TEXT("retained"), retained);
        player_config_.simulation.health =
            ioj::settings::PositiveHealth::make(ioj::sim::Health{432});

        ioj::reset_level_world(*world_, *retained, player, mission, &player_config_);

        if (!TestRunner->TestNotNull(TEXT("Replacement player exists"), player.Get())) {
            return;
        }
        TestRunner->TestTrue(TEXT("Replacement has a new identity"), player.Get() != previous);
        TestRunner->TestTrue(TEXT("Replacement retains the player class"),
                             player->GetClass() == previous_class);
        TestRunner->TestFalse(TEXT("Old player is removed"),
                              ioj::is_actor_in_world(*world_, previous));
        TestRunner->TestTrue(TEXT("Replacement remains in the world"),
                             ioj::is_actor_in_world(*world_, player.Get()));
        TestRunner->TestTrue(TEXT("Reset restores the identity transform"),
                             player->GetActorTransform().Equals(FTransform::Identity));
        TestRunner->TestTrue(TEXT("Mission player reference is replaced"),
                             mission.entity_bindings[TEXT("player")].Get() == player.Get());
        TestRunner->TestTrue(TEXT("Aliased mission references are replaced"),
                             mission.entity_bindings[TEXT("protected-player")].Get() ==
                                 player.Get());
        TestRunner->TestTrue(TEXT("Other mission references are retained"),
                             mission.entity_bindings[TEXT("retained")].Get() == retained);
        TestRunner->TestTrue(TEXT("Replacement uses reset configuration"),
                             player->make_spawn_data().config.health ==
                                 player_config_.simulation.health);
    }

    UWorld* world_{};
    FPlayerShipConfig player_config_;
};
