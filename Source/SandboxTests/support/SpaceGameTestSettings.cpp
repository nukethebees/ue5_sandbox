#include "SpaceGameTestSettings.h"

#include <SandboxTests/SandboxTestLogCategories.h>

#include <Misc/CommandLine.h>
#include <Misc/Parse.h>

USpaceGameTestSettings::USpaceGameTestSettings() {
    CategoryName = TEXT("Sandbox");
    SectionName = TEXT("SpaceGame Tests");
}

namespace ml {
auto get_space_game_level_test_time_scale() -> double {
    double value{};
    if (FParse::Value(FCommandLine::Get(), TEXT("SpaceGameTestTimeScale="), value)) {
        if (value > 0.0) {
            return value;
        }

        UE_LOG(LogSandboxTest,
               Warning,
               TEXT("-SpaceGameTestTimeScale must be positive; using the default"));
    }

    auto const configured{GetDefault<USpaceGameTestSettings>()->level_playback_time_scale};
    return configured > 0.0 ? configured : 100.0;
}
}
