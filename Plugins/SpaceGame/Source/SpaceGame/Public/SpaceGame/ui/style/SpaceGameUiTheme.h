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
    UPROPERTY(EditAnywhere, Category = "Text")
    FTextBlockStyle disabled{};
};

USTRUCT(BlueprintType)
struct SPACEGAME_API FGameButtonStyleDefinition {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Button")
    FButtonStyle normal{};

    UPROPERTY(EditAnywhere, Category = "Button")
    FButtonStyle selected{};

    UPROPERTY(EditAnywhere, Category = "Text")
    EGameTextStyle normal_text{EGameTextStyle::Body};

    UPROPERTY(EditAnywhere, Category = "Text")
    EGameTextStyle normal_hovered_text{EGameTextStyle::Body};

    UPROPERTY(EditAnywhere, Category = "Text")
    EGameTextStyle selected_text{EGameTextStyle::Body};

    UPROPERTY(EditAnywhere, Category = "Text")
    EGameTextStyle selected_hovered_text{EGameTextStyle::Body};

    UPROPERTY(EditAnywhere, Category = "Text")
    EGameTextStyle disabled_text{EGameTextStyle::Disabled};

    UPROPERTY(EditAnywhere, Category = "Layout")
    FMargin custom_padding{};

    UPROPERTY(EditAnywhere, Category = "Layout")
    FVector2f minimum_size{};

    UPROPERTY(EditAnywhere, Category = "Layout")
    FVector2f maximum_size{};
};

USTRUCT(BlueprintType)
struct SPACEGAME_API FGameButtonStyles {
    GENERATED_BODY()

    auto get(EGameButtonStyle role) const -> FGameButtonStyleDefinition const&;

    UPROPERTY(EditAnywhere, Category = "Buttons")
    FGameButtonStyleDefinition primary{};
    UPROPERTY(EditAnywhere, Category = "Buttons")
    FGameButtonStyleDefinition secondary{};
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
