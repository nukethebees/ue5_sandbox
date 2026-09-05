#pragma once

#include "SpaceGame/ui/style/GameUiStyleTypes.h"

#include "SandboxGameShared/utilities/enum_array.h"
#include "SandboxUI/widgets/SettingsWidgets.h"

#include <Styling/SlateTypes.h>

#include "GameUiStyle.generated.h"

namespace ml::ioj {
class USpaceGameUiTheme;

struct SPACEGAME_API FGamePanelStyle {
    FSlateBrush background{};
    FMargin padding{};
};

USTRUCT(BlueprintType)
struct SPACEGAME_API FGameUiPalette {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Surfaces")
    FLinearColor canvas{};
    UPROPERTY(EditAnywhere, Category = "Surfaces")
    FLinearColor surface_low{};
    UPROPERTY(EditAnywhere, Category = "Surfaces")
    FLinearColor surface{};
    UPROPERTY(EditAnywhere, Category = "Surfaces")
    FLinearColor surface_raised{};
    UPROPERTY(EditAnywhere, Category = "Surfaces")
    FLinearColor control{};

    UPROPERTY(EditAnywhere, Category = "Borders")
    FLinearColor border_shadow{};
    UPROPERTY(EditAnywhere, Category = "Borders")
    FLinearColor border{};
    UPROPERTY(EditAnywhere, Category = "Borders")
    FLinearColor border_highlight{};

    UPROPERTY(EditAnywhere, Category = "Text")
    FLinearColor text_primary{};
    UPROPERTY(EditAnywhere, Category = "Text")
    FLinearColor text_secondary{};
    UPROPERTY(EditAnywhere, Category = "Text")
    FLinearColor text_muted{};
    UPROPERTY(EditAnywhere, Category = "Text")
    FLinearColor text_disabled{};

    UPROPERTY(EditAnywhere, Category = "Accent")
    FLinearColor honey{};
    UPROPERTY(EditAnywhere, Category = "Accent")
    FLinearColor honey_hovered{};
    UPROPERTY(EditAnywhere, Category = "Accent")
    FLinearColor honey_pressed{};
    UPROPERTY(EditAnywhere, Category = "Accent")
    FLinearColor focus{};

    UPROPERTY(EditAnywhere, Category = "Status")
    FLinearColor warning{};
    UPROPERTY(EditAnywhere, Category = "Status")
    FLinearColor danger{};
    UPROPERTY(EditAnywhere, Category = "Status")
    FLinearColor success{};
    UPROPERTY(EditAnywhere, Category = "Surfaces")
    FLinearColor modal_overlay{};
};

USTRUCT(BlueprintType)
struct SPACEGAME_API FGameUiTypography {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Typography")
    FSlateFontInfo body{};
    UPROPERTY(EditAnywhere, Category = "Typography")
    FSlateFontInfo display{};
};

struct SPACEGAME_API FGameUiChromeStyle {
    FSlateBrush canvas{};
    FSlateBrush frame_border{};
    FSlateBrush frame_background{};
    FSlateBrush header_background{};
    FSlateBrush navigation_background{};
    FSlateBrush body_background{};
    FSlateBrush footer_background{};
    FSlateBrush focus{};
    FSlateBrush modal_overlay{};
    FScrollBarStyle scroll_bar{};
    FMargin frame_border_thickness{2.0f};
    FMargin frame_padding{1.0f};
    FMargin header_padding{22.0f, 14.0f};
    FMargin footer_padding{16.0f, 12.0f};
    float navigation_width{220.0f};
    float navigation_spacing{8.0f};
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

    UPROPERTY(EditAnywhere, Category = "Button")
    FSlateBrush focus{};

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
    auto settings() const -> FSettingsStyle const&;
    auto palette() const -> FGameUiPalette const&;
    auto chrome() const -> FGameUiChromeStyle const&;
    auto icon(EGameUiIcon role) const -> FSlateBrush const&;
    auto health_bar() const -> FProgressBarStyle const&;
  private:
    friend USpaceGameUiTheme;

    TEnumArray<EGameTextStyle, FTextBlockStyle> text_styles_{};
    TEnumArray<EGameButtonStyle, FGameButtonPresentationStyle> button_styles_{};
    FGamePanelStyle panel_{};
    FSettingsStyle settings_{};
    FGameUiPalette palette_{};
    FGameUiChromeStyle chrome_{};
    TEnumArray<EGameUiIcon, FSlateBrush> icons_{};
    FProgressBarStyle health_bar_{};
};
}
