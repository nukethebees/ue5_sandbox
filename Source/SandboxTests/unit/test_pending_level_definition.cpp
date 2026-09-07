#include <SpaceGame/levels/ExampleLevels.h>
#include <SpaceGame/system/GameSubsystem.h>

#include <CQTest.h>
#include <Engine/GameInstance.h>

#include <limits>

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
        subsystem->set_pending_level(
            ml::example_levels::make_native_example(),
            TEXT("LevelScripts/Example.scm"),
            TEXT("0123456789abcdef"),
            {.launch_mode = ml::ioj::ELevelLaunchMode::Paused,
             .requested_time_scale = 4.0,
             .control_context = EPlayerControlContext::Benchmark});

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
                             pending->options.launch_mode == ml::ioj::ELevelLaunchMode::Paused);
        TestRunner->TestEqual(TEXT("Requested time scale is retained"),
                              pending->options.requested_time_scale,
                              4.0);
        TestRunner->TestTrue(TEXT("Control context is retained"),
                             pending->options.control_context == EPlayerControlContext::Benchmark);
        TestRunner->TestFalse(TEXT("Pending level is consumed exactly once"),
                              subsystem->take_pending_level().IsSet());

        for (auto const value : {0.25, 1.0, 100.0}) {
            TestRunner->TestTrue(TEXT("Supported time scale is accepted"),
                                 ml::ioj::level_launch::is_valid_time_scale(value));
        }
        for (auto const value : {0.0,
                                 -1.0,
                                 100.0001,
                                 std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::quiet_NaN()}) {
            TestRunner->TestFalse(TEXT("Unsupported time scale is rejected"),
                                  ml::ioj::level_launch::is_valid_time_scale(value));
        }
    }
};
