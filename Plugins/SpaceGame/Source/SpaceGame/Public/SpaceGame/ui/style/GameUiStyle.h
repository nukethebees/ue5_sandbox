#pragma once

#include "SpaceGame/ui/style/GameUiStyleTypes.h"

#include "SandboxGameShared/utilities/enum_array.h"

#include <Styling/SlateTypes.h>

namespace ml::ioj {
class USpaceGameUiTheme;

struct SPACEGAME_API FGamePanelStyle {
    FSlateBrush background{};
    FMargin padding{};
};

struct SPACEGAME_API FGameButtonPresentationStyle {
    FButtonStyle normal{};
    FButtonStyle selected{};
    FTextBlockStyle normal_text{};
    FTextBlockStyle normal_hovered_text{};
    FTextBlockStyle selected_text{};
    FTextBlockStyle selected_hovered_text{};
    FTextBlockStyle disabled_text{};
    FMargin custom_padding{};
    FVector2f minimum_size{};
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
