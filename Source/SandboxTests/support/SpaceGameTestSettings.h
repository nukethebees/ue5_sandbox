#pragma once

#include <CoreMinimal.h>
#include <Engine/DeveloperSettings.h>

#include "SpaceGameTestSettings.generated.h"

UCLASS(Config = EditorPerProjectUserSettings)
class USpaceGameTestSettings : public UDeveloperSettings {
    GENERATED_BODY()
  public:
    USpaceGameTestSettings();

    UPROPERTY(Config, EditAnywhere, Category = "SpaceGame", meta = (ClampMin = "0.01"))
    double level_playback_time_scale{100.0};
};

namespace ml {
auto get_space_game_level_test_time_scale() -> double;
}
