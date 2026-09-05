#pragma once

#include "SpaceGame/ui/style/GameUiStyleTypes.h"

#include "SandboxGameShared/utilities/enum_array.h"

#include <Styling/SlateTypes.h>

#include "GameUiStyle.generated.h"

namespace ml::ioj {
class USpaceGameUiTheme;

struct SPACEGAME_API FGamePanelStyle {
    FSlateBrush background{};
    FMargin padding{};
};

USTRUCT(BlueprintType)
struct SPACEGAME_API FGameButtonPresentationStyle {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Button")
    FButtonStyle normal{};

    UPROPERTY(EditAnywhere, Category = "Button")
    FButtonStyle selected{};

    UPROPERTY(EditAnywhere, Category = "Text")
    FTextBlockStyle normal_text{};

    UPROPERTY(EditAnywhere, Category = "Text")
    FTextBlockStyle normal_hovered_text{};

    UPROPERTY(EditAnywhere, Category = "Text")
    FTextBlockStyle selected_text{};

    UPROPERTY(EditAnywhere, Category = "Text")
    FTextBlockStyle selected_hovered_text{};

    UPROPERTY(EditAnywhere, Category = "Text")
    FTextBlockStyle disabled_text{};

    UPROPERTY(EditAnywhere, Category = "Layout")
    FMargin custom_padding{};

    UPROPERTY(EditAnywhere, Category = "Layout")
    FVector2f minimum_size{};

    UPROPERTY(EditAnywhere, Category = "Layout")
    FVector2f maximum_size{};
};

class SPACEGAME_API FGameUiStyle {
  public:
    auto text(EGameTextStyle role) const -> FTextBlockStyle const&;
    auto button(EGameButtonStyle role) const -> FGameButtonPresentationStyle const&;
    auto panel() const -> FGamePanelStyle const&;
    auto health_bar() const -> FProgressBarStyle const&;
  private:
    friend USpaceGameUiTheme;

    TEnumArray<EGameTextStyle, FTextBlockStyle> text_styles_{};
    TEnumArray<EGameButtonStyle, FGameButtonPresentationStyle> button_styles_{};
    FGamePanelStyle panel_{};
    FProgressBarStyle health_bar_{};
};
}
