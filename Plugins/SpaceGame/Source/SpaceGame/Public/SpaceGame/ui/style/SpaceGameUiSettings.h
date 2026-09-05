#pragma once

#include <Engine/DeveloperSettings.h>

#include "SpaceGameUiSettings.generated.h"

namespace ml::ioj {
class USpaceGameUiTheme;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Space Game UI"))
class SPACEGAME_API USpaceGameUiSettings : public UDeveloperSettings {
    GENERATED_BODY()
  public:
    USpaceGameUiSettings();

    UPROPERTY(Config, EditAnywhere, Category = "Theme")
    TSoftObjectPtr<USpaceGameUiTheme> default_theme{};
};
}
