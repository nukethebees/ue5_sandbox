#include <SpaceGame/levels/ExampleLevels.h>
#include <SpaceGame/system/GameSubsystem.h>

#include <CQTest.h>
#include <Engine/GameInstance.h>

TEST_CLASS(PendingLevelDefinition, "Sandbox.UnitTests")
{
    TEST_METHOD(IsTransferredToTheRuntimeExactlyOnce)
    {
        auto* const game_instance{NewObject<UGameInstance>()};
        auto* const subsystem{NewObject<ml::ioj::UGameSubsystem>(game_instance)};
        if (!TestRunner->TestTrue(TEXT("Game subsystem is created"), IsValid(subsystem))) {
            return;
        }

        subsystem->set_level_launch_error(TEXT("old error"));
        subsystem->set_pending_level(ml::example_levels::make_native_example(),
                                     TEXT("LevelScripts/Example.scm"),
                                     ml::ioj::ELevelLaunchMode::Paused);

        TestRunner->TestFalse(TEXT("Selecting a new level clears an old launch error"),
                              subsystem->has_level_launch_error());

        auto const pending{subsystem->take_pending_level()};
        if (!TestRunner->TestTrue(TEXT("Pending level is available"), pending.IsSet())) {
            return;
        }
        TestRunner->TestEqual(TEXT("Source path is retained"),
                              pending->source_path,
                              FString{TEXT("LevelScripts/Example.scm")});
        TestRunner->TestEqual(TEXT("Native definition is retained"),
                              pending->definition.metadata.title,
                              FString{TEXT("Native Example")});
        TestRunner->TestTrue(TEXT("Launch mode is retained"),
                             pending->launch_mode == ml::ioj::ELevelLaunchMode::Paused);
        TestRunner->TestFalse(TEXT("Pending level is consumed exactly once"),
                              subsystem->take_pending_level().IsSet());
    }
};
