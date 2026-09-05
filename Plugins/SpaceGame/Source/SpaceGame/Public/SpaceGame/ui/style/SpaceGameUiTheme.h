#pragma once

#include "SpaceGame/ui/style/GameUiStyle.h"

#include <Engine/DataAsset.h>

#include "SpaceGameUiTheme.generated.h"

namespace ml::ioj {
USTRUCT(BlueprintType)
struct SPACEGAME_API FGameTextStyles {
    GENERATED_BODY()

    auto get(EGameTextStyle role) const -> FTextBlockStyle const&;

    UPROPERTY(EditAnywhere, Category = "Text")
    FTextBlockStyle body{};
    UPROPERTY(EditAnywhere, Category = "Text")
    FTextBlockStyle body_secondary{};
    UPROPERTY(EditAnywhere, Category = "Text")
    FTextBlockStyle caption{};
    UPROPERTY(EditAnywhere, Category = "Text")
    FTextBlockStyle heading_1{};
    UPROPERTY(EditAnywhere, Category = "Text")
    FTextBlockStyle heading_2{};
    UPROPERTY(EditAnywhere, Category = "Text")
    FTextBlockStyle heading_3{};
    UPROPERTY(EditAnywhere, Category = "Text")
    FTextBlockStyle warning{};
    UPROPERTY(EditAnywhere, Category = "Text")
    FTextBlockStyle hud_primary{};
    UPROPERTY(EditAnywhere, Category = "Text")
    FTextBlockStyle hud_secondary{};
};

USTRUCT(BlueprintType)
struct SPACEGAME_API FGameButtonStyles {
    GENERATED_BODY()

    auto get(EGameButtonStyle role) const -> FGameButtonPresentationStyle const&;

    UPROPERTY(EditAnywhere, Category = "Buttons")
    FGameButtonPresentationStyle primary{};
    UPROPERTY(EditAnywhere, Category = "Buttons")
    FGameButtonPresentationStyle secondary{};
};

UCLASS(BlueprintType)
class SPACEGAME_API USpaceGameUiTheme : public UDataAsset {
    GENERATED_BODY()
  public:
    USpaceGameUiTheme();

    auto compile() const -> FGameUiStyle;
  private:
    UPROPERTY(EditAnywhere, Category = "Text")
    FGameTextStyles text_styles_{};

    UPROPERTY(EditAnywhere, Category = "Buttons")
    FGameButtonStyles button_styles_{};

    UPROPERTY(EditAnywhere, Category = "Panels")
    FSlateBrush panel_background_{};

    UPROPERTY(EditAnywhere, Category = "Panels")
    FMargin panel_padding_{16.f};

    UPROPERTY(EditAnywhere, Category = "HUD")
    FProgressBarStyle health_bar_{};
};
}
