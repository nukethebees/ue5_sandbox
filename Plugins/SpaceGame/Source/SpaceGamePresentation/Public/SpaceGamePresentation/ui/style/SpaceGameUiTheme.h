#pragma once

#include "SpaceGamePresentation/ui/style/GameUiStyle.h"

#include <Engine/DataAsset.h>

#include "SpaceGameUiTheme.generated.h"

namespace ml::ioj {
UCLASS(BlueprintType)
class SPACEGAMEPRESENTATION_API USpaceGameUiTheme : public UDataAsset {
    GENERATED_BODY()
  public:
    USpaceGameUiTheme();

    auto compile() const -> FGameUiStyle;
  private:
    UPROPERTY(EditAnywhere, Category = "Theme")
    FGameUiPalette palette_{};

    UPROPERTY(EditAnywhere, Category = "Theme")
    FGameUiTypography typography_{};

    UPROPERTY(EditAnywhere, Category = "Panels")
    FMargin panel_padding_{16.f};

    UPROPERTY(EditAnywhere, Category = "Settings")
    FSettingsStyle settings_style_{};
};
}
