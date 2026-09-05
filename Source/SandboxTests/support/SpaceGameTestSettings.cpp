#include "SpaceGameTestSettings.h"

#include <SandboxTests/SandboxTestLogCategories.h>

#include <Misc/CommandLine.h>
#include <Misc/Parse.h>

USpaceGameTestSettings::USpaceGameTestSettings() {
    CategoryName = TEXT("Sandbox");
    SectionName = TEXT("SpaceGame Tests");
}

namespace ml {
auto get_space_game_test_execution_mode() -> ESpaceGameTestExecutionMode {
    FString value;
    if (FParse::Value(FCommandLine::Get(), TEXT("SpaceGameTestMode="), value)) {
        if (value.Equals(TEXT("Headless"), ESearchCase::IgnoreCase)) {
            return ESpaceGameTestExecutionMode::Headless;
        }
        if (value.Equals(TEXT("Level"), ESearchCase::IgnoreCase)) {
            return ESpaceGameTestExecutionMode::Level;
        }

        UE_LOG(LogSandboxTest,
               Warning,
               TEXT("Unknown -SpaceGameTestMode='%s'; using the developer setting"),
               *value);
    }

    return GetDefault<USpaceGameTestSettings>()->execution_mode;
}

auto get_space_game_level_test_time_scale(bool const explicit_level_mode) -> double {
    double value{};
    if (FParse::Value(FCommandLine::Get(), TEXT("SpaceGameTestTimeScale="), value)) {
        if (value > 0.0) {
            return value;
        }

        UE_LOG(LogSandboxTest,
               Warning,
               TEXT("-SpaceGameTestTimeScale must be positive; using the default"));
    }

    if (!explicit_level_mode) {
        return 100.0;
    }

    auto const configured{GetDefault<USpaceGameTestSettings>()->level_playback_time_scale};
    return configured > 0.0 ? configured : 1.0;
}
}
