#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/LevelWorldReset.h>

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

        ioj::reset_level_world(*world_, *retained);

        TestRunner->TestTrue(TEXT("Retained actor remains in the world"),
                             ioj::is_actor_in_world(*world_, retained));
        TestRunner->TestTrue(TEXT("Framework actor remains in the world"),
                             ioj::is_actor_in_world(*world_, volume));
        TestRunner->TestFalse(TEXT("Transient actor is removed"),
                              ioj::is_actor_in_world(*world_, transient));
    }

    TEST_METHOD(RemovesPlayerForTheNextAuthoredLoad)
    {
        auto* const retained{world_->SpawnActor<AActor>()};
        auto* const player{world_->SpawnActor<ioj::ATestSpaceShip>()};
        if (!TestRunner->TestNotNull(TEXT("Retained actor exists"), retained) ||
            !TestRunner->TestNotNull(TEXT("Player exists"), player)) {
            return;
        }

        ioj::reset_level_world(*world_, *retained);

        TestRunner->TestFalse(TEXT("Previous player is removed"),
                              ioj::is_actor_in_world(*world_, player));
        TestRunner->TestEqual(TEXT("Reset leaves player construction to the level loader"),
                              ioj::count_actors<ioj::ATestSpaceShip>(*world_),
                              0);
    }

    UWorld* world_{};
};
