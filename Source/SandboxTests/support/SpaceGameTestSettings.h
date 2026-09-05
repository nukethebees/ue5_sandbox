#pragma once

#include <CoreMinimal.h>
#include <Engine/DeveloperSettings.h>

#include "SpaceGameTestSettings.generated.h"

UENUM()
enum class ESpaceGameTestExecutionMode : uint8 {
    Headless,
    Level,
};

UCLASS(Config = EditorPerProjectUserSettings)
class USpaceGameTestSettings : public UDeveloperSettings {
    GENERATED_BODY()
  public:
    USpaceGameTestSettings();

    UPROPERTY(Config, EditAnywhere, Category = "SpaceGame")
    ESpaceGameTestExecutionMode execution_mode{ESpaceGameTestExecutionMode::Headless};

    UPROPERTY(Config, EditAnywhere, Category = "SpaceGame", meta = (ClampMin = "0.01"))
    double level_playback_time_scale{1.0};
};

namespace ml {
auto get_space_game_test_execution_mode() -> ESpaceGameTestExecutionMode;
auto get_space_game_level_test_time_scale(bool explicit_level_mode) -> double;
}
