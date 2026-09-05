#pragma once

#include "SpaceGame/ui/style/GameUiStyle.h"

#include <CommonButtonBase.h>
#include <CommonTextBlock.h>
#include <Engine/DataAsset.h>

#include "SpaceGameUiTheme.generated.h"

namespace ml::ioj {
USTRUCT(BlueprintType)
struct SPACEGAME_API FGameTextStyleClasses {
    GENERATED_BODY()

    auto get(EGameTextStyle role) const -> TSubclassOf<UCommonTextStyle>;

    UPROPERTY(EditAnywhere, Category = "Text")
    TSubclassOf<UCommonTextStyle> body{};
    UPROPERTY(EditAnywhere, Category = "Text")
    TSubclassOf<UCommonTextStyle> body_secondary{};
    UPROPERTY(EditAnywhere, Category = "Text")
    TSubclassOf<UCommonTextStyle> caption{};
    UPROPERTY(EditAnywhere, Category = "Text")
    TSubclassOf<UCommonTextStyle> heading_1{};
    UPROPERTY(EditAnywhere, Category = "Text")
    TSubclassOf<UCommonTextStyle> heading_2{};
    UPROPERTY(EditAnywhere, Category = "Text")
    TSubclassOf<UCommonTextStyle> heading_3{};
    UPROPERTY(EditAnywhere, Category = "Text")
    TSubclassOf<UCommonTextStyle> warning{};
    UPROPERTY(EditAnywhere, Category = "Text")
    TSubclassOf<UCommonTextStyle> hud_primary{};
    UPROPERTY(EditAnywhere, Category = "Text")
    TSubclassOf<UCommonTextStyle> hud_secondary{};
};

USTRUCT(BlueprintType)
struct SPACEGAME_API FGameButtonStyleClasses {
    GENERATED_BODY()

    auto get(EGameButtonStyle role) const -> TSubclassOf<UCommonButtonStyle>;

    UPROPERTY(EditAnywhere, Category = "Buttons")
    TSubclassOf<UCommonButtonStyle> primary{};
    UPROPERTY(EditAnywhere, Category = "Buttons")
    TSubclassOf<UCommonButtonStyle> secondary{};
};

UCLASS(BlueprintType)
class SPACEGAME_API USpaceGameUiTheme : public UDataAsset {
    GENERATED_BODY()
  public:
    USpaceGameUiTheme();

    auto compile(FGameUiStyle& result) const -> bool;
    auto get_common_text_style(EGameTextStyle role) const -> TSubclassOf<UCommonTextStyle>;
    auto get_common_button_style(EGameButtonStyle role) const -> TSubclassOf<UCommonButtonStyle>;
    void set_common_text_style(EGameTextStyle role, TSubclassOf<UCommonTextStyle> style);
    void set_common_button_style(EGameButtonStyle role, TSubclassOf<UCommonButtonStyle> style);
  private:
    UPROPERTY(EditAnywhere, Category = "Text")
    FGameTextStyleClasses text_styles_{};

    UPROPERTY(EditAnywhere, Category = "Buttons")
    FGameButtonStyleClasses button_styles_{};

    UPROPERTY(EditAnywhere, Category = "Panels")
    FSlateBrush panel_background_{};

    UPROPERTY(EditAnywhere, Category = "Panels")
    FMargin panel_padding_{16.f};

    UPROPERTY(EditAnywhere, Category = "HUD")
    FProgressBarStyle health_bar_{};
};
}
