#pragma once

#include "SpaceGamePresentation/ui/style/GameUiStyleTypes.h"

#include "SandboxCore/enum_array.h"
#include "SandboxUI/widgets/SettingsWidgets.h"
#include "SandboxUI/widgets/SGraphPlot.h"

#include <Styling/SlateTypes.h>

#include "GameUiStyle.generated.h"

class UTextBlock;
class SWidget;

namespace ml::ioj {
class USpaceGameUiTheme;

struct SPACEGAMEPRESENTATION_API FGameHudStyle {
    FTextBlockStyle heading_text{};
    FTextBlockStyle primary_text{};
    FTextBlockStyle secondary_text{};
    FTextBlockStyle caption_text{};
    FTextBlockStyle data_text{};
    FTextBlockStyle data_accent_text{};
    FTextBlockStyle accent_text{};
    FTextBlockStyle success_text{};
    FTextBlockStyle warning_text{};
    FTextBlockStyle danger_text{};

    FSlateBrush panel_background{};
    FSlateBrush table_header_background{};
    FSlateBrush table_row_background{};
    FSlateBrush table_alternate_row_background{};
    FSlateBrush control_background{};
    FSlateBrush border{};
    FProgressBarStyle health_bar{};
    FProgressBarStyle energy_bar{};
    FGraphPlotStyle graph{};

    FLinearColor health_nominal{};
    FLinearColor health_warning{};
    FLinearColor health_critical{};
    FLinearColor energy{};
    FLinearColor reticle_normal{};
    FLinearColor reticle_warning{};
    FLinearColor reticle_danger{};
    FLinearColor objective_defend{};
    FLinearColor objective_destroy{};
    FLinearColor graph_series{};

    FMargin panel_padding{10.0f};
    FMargin table_cell_padding{6.0f, 3.0f};
    FVector2f force_status_bar_size{18.0f, 104.0f};
    float force_status_bar_spacing{4.0f};
    float team_wash_opacity{0.08f};
    float team_text_blend{0.20f};
    float health_warning_threshold{0.50f};
    float health_critical_threshold{0.25f};
};

struct SPACEGAMEPRESENTATION_API FGamePanelStyle {
    FSlateBrush background{};
    FMargin padding{};
};

USTRUCT(BlueprintType)
struct SPACEGAMEPRESENTATION_API FGameUiPalette {
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
    UPROPERTY(EditAnywhere, Category = "Status")
    FLinearColor completion{};
    UPROPERTY(EditAnywhere, Category = "Surfaces")
    FLinearColor modal_overlay{};
};

USTRUCT(BlueprintType)
struct SPACEGAMEPRESENTATION_API FGameUiTypography {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Typography")
    FSlateFontInfo body{};
    UPROPERTY(EditAnywhere, Category = "Typography")
    FSlateFontInfo display{};
};

struct SPACEGAMEPRESENTATION_API FGameUiChromeStyle {
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
struct SPACEGAMEPRESENTATION_API FGameButtonPresentationStyle {
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

class SPACEGAMEPRESENTATION_API FGameUiStyle {
  public:
    auto text(EGameTextStyle role) const -> FTextBlockStyle const&;
    auto button(EGameButtonStyle role) const -> FGameButtonPresentationStyle const&;
    auto panel() const -> FGamePanelStyle const&;
    auto settings() const -> FSettingsStyle const&;
    auto palette() const -> FGameUiPalette const&;
    auto chrome() const -> FGameUiChromeStyle const&;
    auto icon(EGameUiIcon role) const -> FSlateBrush const&;
    auto hud() const -> FGameHudStyle const&;
  private:
    friend USpaceGameUiTheme;

    TEnumArray<EGameTextStyle, FTextBlockStyle> text_styles_{};
    TEnumArray<EGameButtonStyle, FGameButtonPresentationStyle> button_styles_{};
    FGamePanelStyle panel_{};
    FSettingsStyle settings_{};
    FGameUiPalette palette_{};
    FGameUiChromeStyle chrome_{};
    TEnumArray<EGameUiIcon, FSlateBrush> icons_{};
    FGameHudStyle hud_{};
};

SPACEGAMEPRESENTATION_API void apply_text_style(UTextBlock& text, FTextBlockStyle const& style);
SPACEGAMEPRESENTATION_API auto make_hud_panel(FGameHudStyle const& style,
                                              TSharedRef<SWidget> content) -> TSharedRef<SWidget>;
}
