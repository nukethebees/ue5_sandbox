#include "SpaceGame/ui/style/SpaceGameUiTheme.h"

#include <Brushes/SlateImageBrush.h>
#include <Interfaces/IPluginManager.h>
#include <Misc/Paths.h>
#include <Styling/CoreStyle.h>

namespace ml::ioj {
namespace {
auto srgb(TCHAR const* const value) -> FLinearColor {
    return FLinearColor::FromSRGBColor(FColor::FromHex(value));
}

auto make_brush(FLinearColor const& colour, FVector2D const size = FVector2D{16.0f, 16.0f})
    -> FSlateBrush {
    FSlateBrush brush;
    brush.DrawAs = ESlateBrushDrawType::Box;
    brush.ImageSize = size;
    brush.TintColor = FSlateColor{colour};
    return brush;
}

auto make_text_style(FSlateFontInfo font, int32 const size, FLinearColor const& colour)
    -> FTextBlockStyle {
    font.Size = size;
    return FTextBlockStyle{}.SetFont(font).SetColorAndOpacity(FSlateColor{colour});
}

auto make_button_state_style(FLinearColor const& normal,
                             FLinearColor const& hovered,
                             FLinearColor const& pressed,
                             FLinearColor const& disabled) -> FButtonStyle {
    return FButtonStyle{}
        .SetNormal(make_brush(normal))
        .SetHovered(make_brush(hovered))
        .SetPressed(make_brush(pressed))
        .SetDisabled(make_brush(disabled))
        .SetNormalPadding(FMargin{14.0f, 7.0f})
        .SetPressedPadding(FMargin{14.0f, 8.0f, 14.0f, 6.0f});
}

auto make_button_style(FGameUiPalette const& palette,
                       FTextBlockStyle const& normal_text,
                       FTextBlockStyle const& hovered_text,
                       FTextBlockStyle const& selected_text,
                       FTextBlockStyle const& disabled_text,
                       bool const primary) -> FGameButtonPresentationStyle {
    auto result{FGameButtonPresentationStyle{}};
    if (primary) {
        result.normal = make_button_state_style(
            palette.honey, palette.honey_hovered, palette.honey_pressed, palette.surface_raised);
        result.selected = result.normal;
    } else {
        result.normal = make_button_state_style(
            palette.surface_raised, palette.border, palette.surface, palette.surface_low);
        result.selected = make_button_state_style(
            palette.honey, palette.honey_hovered, palette.honey_pressed, palette.surface_low);
    }
    result.normal_text = normal_text;
    result.normal_hovered_text = hovered_text;
    result.selected_text = selected_text;
    result.selected_hovered_text = selected_text;
    result.disabled_text = disabled_text;
    result.focus = make_brush(palette.focus);
    result.minimum_size = FVector2f{0.0f, primary ? 42.0f : 40.0f};
    return result;
}

auto make_scroll_bar_style(FGameUiPalette const& palette) -> FScrollBarStyle {
    auto const track{make_brush(palette.control, FVector2D{10.0f, 10.0f})};
    auto const thumb{make_brush(palette.honey, FVector2D{10.0f, 10.0f})};
    auto const hovered{make_brush(palette.honey_hovered, FVector2D{10.0f, 10.0f})};
    return FScrollBarStyle{}
        .SetHorizontalBackgroundImage(track)
        .SetVerticalBackgroundImage(track)
        .SetHorizontalTopSlotImage(track)
        .SetHorizontalBottomSlotImage(track)
        .SetVerticalTopSlotImage(track)
        .SetVerticalBottomSlotImage(track)
        .SetNormalThumbImage(thumb)
        .SetHoveredThumbImage(hovered)
        .SetDraggedThumbImage(hovered)
        .SetThickness(10.0f);
}

auto make_progress_bar_style(FLinearColor const& background) -> FProgressBarStyle {
    auto const background_brush{make_brush(background, FVector2D{8.0f, 8.0f})};
    auto const fill_brush{make_brush(FLinearColor::White, FVector2D{8.0f, 8.0f})};
    return FProgressBarStyle{}
        .SetBackgroundImage(background_brush)
        .SetFillImage(fill_brush)
        .SetMarqueeImage(fill_brush);
}

auto icon_path(EGameUiIcon const icon) -> FString {
    auto const plugin{IPluginManager::Get().FindPlugin(TEXT("SpaceGame"))};
    auto const root{plugin.IsValid()
                        ? plugin->GetBaseDir()
                        : FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("SpaceGame"))};
    return FPaths::Combine(
        root, TEXT("Resources/UI/Hive"), FString::Printf(TEXT("%s.svg"), LexToString(icon)));
}

auto make_icon(EGameUiIcon const icon) -> FSlateBrush {
    return FSlateVectorImageBrush{icon_path(icon), FVector2D{24.0f, 24.0f}, FLinearColor::White};
}
}

USpaceGameUiTheme::USpaceGameUiTheme() {
    palette_.canvas = srgb(TEXT("10130F"));
    palette_.surface_low = srgb(TEXT("181B16"));
    palette_.surface = srgb(TEXT("25271F"));
    palette_.surface_raised = srgb(TEXT("33342A"));
    palette_.control = srgb(TEXT("171914"));
    palette_.border_shadow = srgb(TEXT("090A08"));
    palette_.border = srgb(TEXT("6F7064"));
    palette_.border_highlight = srgb(TEXT("A7A596"));
    palette_.text_primary = srgb(TEXT("E5E0D2"));
    palette_.text_secondary = srgb(TEXT("B7B2A4"));
    palette_.text_muted = srgb(TEXT("7E7B70"));
    palette_.text_disabled = srgb(TEXT("66645D"));
    palette_.honey = srgb(TEXT("D6A73B"));
    palette_.honey_hovered = srgb(TEXT("EBC45D"));
    palette_.honey_pressed = srgb(TEXT("A87922"));
    palette_.focus = srgb(TEXT("F3CD66"));
    palette_.warning = srgb(TEXT("D98B37"));
    palette_.danger = srgb(TEXT("C45D4C"));
    palette_.success = srgb(TEXT("7E9E62"));
    palette_.modal_overlay = srgb(TEXT("050604")).CopyWithNewOpacity(0.85f);

    typography_.body = FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), 14);
    typography_.display = FCoreStyle::GetDefaultFontStyle(TEXT("BoldCondensed"), 18);
}

auto USpaceGameUiTheme::compile() const -> FGameUiStyle {
    FGameUiStyle compiled;
    compiled.palette_ = palette_;

    compiled.text_styles_[EGameTextStyle::Body] =
        make_text_style(typography_.body, 14, palette_.text_primary);
    compiled.text_styles_[EGameTextStyle::BodySecondary] =
        make_text_style(typography_.body, 14, palette_.text_secondary);
    compiled.text_styles_[EGameTextStyle::Caption] =
        make_text_style(typography_.body, 12, palette_.text_muted);
    compiled.text_styles_[EGameTextStyle::Heading1] =
        make_text_style(typography_.display, 30, palette_.text_primary);
    compiled.text_styles_[EGameTextStyle::Heading2] =
        make_text_style(typography_.display, 24, palette_.text_primary);
    compiled.text_styles_[EGameTextStyle::Heading3] =
        make_text_style(typography_.display, 19, palette_.text_primary);
    compiled.text_styles_[EGameTextStyle::Warning] =
        make_text_style(typography_.display, 14, palette_.warning);
    compiled.text_styles_[EGameTextStyle::HudPrimary] =
        make_text_style(typography_.display, 18, palette_.text_primary);
    compiled.text_styles_[EGameTextStyle::HudSecondary] =
        make_text_style(typography_.body, 14, palette_.text_secondary);
    compiled.text_styles_[EGameTextStyle::Disabled] =
        make_text_style(typography_.body, 14, palette_.text_disabled);

    auto const dark_button_text{make_text_style(typography_.display, 16, palette_.control)};
    auto const normal_button_text{make_text_style(typography_.display, 16, palette_.text_primary)};
    auto const hovered_button_text{make_text_style(typography_.display, 16, palette_.text_primary)};
    auto const disabled_button_text{
        make_text_style(typography_.display, 16, palette_.text_disabled)};
    compiled.button_styles_[EGameButtonStyle::Primary] = make_button_style(
        palette_, dark_button_text, dark_button_text, dark_button_text, disabled_button_text, true);
    compiled.button_styles_[EGameButtonStyle::Secondary] = make_button_style(palette_,
                                                                             normal_button_text,
                                                                             hovered_button_text,
                                                                             dark_button_text,
                                                                             disabled_button_text,
                                                                             false);

    auto settings{settings_style_};
    settings.section_text = make_text_style(typography_.display, 19, palette_.text_primary);
    settings.label_text = compiled.text_styles_[EGameTextStyle::Body];
    settings.value_text = compiled.text_styles_[EGameTextStyle::BodySecondary];
    settings.disabled_text = compiled.text_styles_[EGameTextStyle::Disabled];
    settings.empty_text = compiled.text_styles_[EGameTextStyle::BodySecondary];
    settings.page_background = make_brush(palette_.canvas);
    settings.section_background = make_brush(palette_.surface);
    settings.section_border = make_brush(palette_.border);
    settings.value_background = make_brush(palette_.control);
    settings.page_margin = FMargin{32.0f};
    settings.body_padding = FMargin{16.0f};
    settings.section_padding = FMargin{18.0f};
    settings.section_border_thickness = FMargin{1.0f};
    settings.section_title_padding = FMargin{0.0f, 0.0f, 0.0f, 14.0f};
    settings.row_padding = FMargin{0.0f, 5.0f};
    settings.section_spacing = 16.0f;
    settings.header_spacing = 0.0f;
    settings.footer_spacing = 0.0f;
    settings.tab_spacing = 8.0f;
    settings.button_spacing = 12.0f;
    settings.window_size = FVector2f{1240.0f, 760.0f};
    settings.label_width = 280.0f;
    settings.control_width = 440.0f;
    settings.row_minimum_height = 38.0f;
    settings.label_control_spacing = 28.0f;
    settings.value_text_width = 72.0f;
    settings.value_text_spacing = 12.0f;

    auto const normal_bar{make_brush(palette_.border, FVector2D{8.0f, 4.0f})};
    auto const hovered_bar{make_brush(palette_.border_highlight, FVector2D{8.0f, 4.0f})};
    auto const disabled_bar{make_brush(palette_.text_disabled, FVector2D{8.0f, 4.0f})};
    auto const normal_thumb{make_brush(palette_.honey, FVector2D{18.0f, 18.0f})};
    auto const hovered_thumb{make_brush(palette_.honey_hovered, FVector2D{20.0f, 20.0f})};
    auto const disabled_thumb{make_brush(palette_.text_disabled, FVector2D{18.0f, 18.0f})};
    settings.slider = FSliderStyle{}
                          .SetNormalBarImage(normal_bar)
                          .SetHoveredBarImage(hovered_bar)
                          .SetDisabledBarImage(disabled_bar)
                          .SetNormalThumbImage(normal_thumb)
                          .SetHoveredThumbImage(hovered_thumb)
                          .SetDisabledThumbImage(disabled_thumb)
                          .SetBarThickness(4.0f);

    auto const unchecked{make_brush(palette_.control, FVector2D{22.0f, 22.0f})};
    auto const unchecked_hovered{make_brush(palette_.border, FVector2D{22.0f, 22.0f})};
    auto const checked{make_brush(palette_.honey, FVector2D{22.0f, 22.0f})};
    auto const checked_hovered{make_brush(palette_.honey_hovered, FVector2D{22.0f, 22.0f})};
    settings.toggle =
        FCheckBoxStyle{}
            .SetCheckBoxType(ESlateCheckBoxType::CheckBox)
            .SetUncheckedImage(unchecked)
            .SetUncheckedHoveredImage(unchecked_hovered)
            .SetUncheckedPressedImage(unchecked_hovered)
            .SetCheckedImage(checked)
            .SetCheckedHoveredImage(checked_hovered)
            .SetCheckedPressedImage(make_brush(palette_.honey_pressed, FVector2D{22.0f, 22.0f}));

    auto combo_button{
        FCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>(TEXT("ComboBox")).ComboButtonStyle};
    combo_button.SetButtonStyle(make_button_state_style(
        palette_.control, palette_.surface_raised, palette_.surface, palette_.surface_low));
    combo_button.DownArrowImage.TintColor = FSlateColor{palette_.text_secondary};
    combo_button.SetMenuBorderBrush(make_brush(palette_.border));
    combo_button.SetMenuBorderPadding(FMargin{1.0f});
    settings.combo_box = FComboBoxStyle{}
                             .SetComboButtonStyle(combo_button)
                             .SetContentPadding(FMargin{10.0f, 5.0f})
                             .SetMenuRowPadding(FMargin{10.0f, 5.0f});
    auto const row{make_brush(palette_.surface)};
    auto const row_hovered{make_brush(palette_.surface_raised)};
    auto const row_selected{make_brush(palette_.honey_pressed)};
    settings.combo_row = FTableRowStyle{}
                             .SetEvenRowBackgroundBrush(row)
                             .SetOddRowBackgroundBrush(row)
                             .SetEvenRowBackgroundHoveredBrush(row_hovered)
                             .SetOddRowBackgroundHoveredBrush(row_hovered)
                             .SetActiveBrush(row_selected)
                             .SetActiveHoveredBrush(row_selected)
                             .SetInactiveBrush(row_selected)
                             .SetInactiveHoveredBrush(row_selected)
                             .SetSelectorFocusedBrush(make_brush(palette_.focus))
                             .SetTextColor(FSlateColor{palette_.text_primary})
                             .SetSelectedTextColor(FSlateColor{palette_.text_primary});
    settings.scroll_bar = make_scroll_bar_style(palette_);
    compiled.settings_ = MoveTemp(settings);

    compiled.chrome_.canvas = make_brush(palette_.canvas);
    compiled.chrome_.frame_border = make_brush(palette_.border_highlight);
    compiled.chrome_.frame_background = make_brush(palette_.surface_low);
    compiled.chrome_.header_background = make_brush(palette_.surface_raised);
    compiled.chrome_.navigation_background = make_brush(palette_.surface_low);
    compiled.chrome_.body_background = make_brush(palette_.surface);
    compiled.chrome_.footer_background = make_brush(palette_.surface_raised);
    compiled.chrome_.focus = make_brush(palette_.focus);
    compiled.chrome_.modal_overlay = make_brush(palette_.modal_overlay);
    compiled.chrome_.scroll_bar = make_scroll_bar_style(palette_);

    for (int32 index{}; index < TEnumTraits<EGameUiIcon>::count; ++index) {
        auto const icon{static_cast<EGameUiIcon>(index)};
        compiled.icons_[icon] = make_icon(icon);
    }

    compiled.panel_ = FGamePanelStyle{
        .background = make_brush(palette_.surface.CopyWithNewOpacity(0.88f)),
        .padding = panel_padding_,
    };

    auto& hud{compiled.hud_};
    hud.heading_text = compiled.text_styles_[EGameTextStyle::HudPrimary];
    hud.heading_text.SetTransformPolicy(ETextTransformPolicy::ToUpper);
    hud.primary_text = compiled.text_styles_[EGameTextStyle::HudPrimary];
    hud.secondary_text = compiled.text_styles_[EGameTextStyle::HudSecondary];
    hud.caption_text = compiled.text_styles_[EGameTextStyle::Caption];
    hud.caption_text.SetTransformPolicy(ETextTransformPolicy::ToUpper);
    hud.data_text = compiled.text_styles_[EGameTextStyle::Body];
    hud.data_accent_text = hud.data_text;
    hud.data_accent_text.SetColorAndOpacity(FSlateColor{palette_.honey});
    hud.accent_text = hud.primary_text;
    hud.accent_text.SetColorAndOpacity(FSlateColor{palette_.honey});
    hud.success_text = hud.secondary_text;
    hud.success_text.SetColorAndOpacity(FSlateColor{palette_.success});
    hud.warning_text = hud.secondary_text;
    hud.warning_text.SetColorAndOpacity(FSlateColor{palette_.warning});
    hud.danger_text = hud.secondary_text;
    hud.danger_text.SetColorAndOpacity(FSlateColor{palette_.danger});

    hud.panel_background = make_brush(palette_.surface_low.CopyWithNewOpacity(0.88f));
    hud.table_header_background = make_brush(palette_.surface_raised.CopyWithNewOpacity(0.88f));
    hud.table_row_background = make_brush(palette_.surface.CopyWithNewOpacity(0.82f));
    hud.table_alternate_row_background = make_brush(palette_.surface_low.CopyWithNewOpacity(0.82f));
    hud.control_background = make_brush(palette_.control.CopyWithNewOpacity(0.92f));
    hud.border = make_brush(palette_.border.CopyWithNewOpacity(0.55f));
    hud.health_bar = make_progress_bar_style(palette_.control.CopyWithNewOpacity(0.92f));
    hud.energy_bar = make_progress_bar_style(palette_.control.CopyWithNewOpacity(0.92f));

    hud.health_nominal = palette_.success;
    hud.health_warning = palette_.warning;
    hud.health_critical = palette_.danger;
    hud.energy = palette_.honey;
    hud.reticle_normal = palette_.success;
    hud.reticle_warning = palette_.warning;
    hud.reticle_danger = palette_.danger;
    hud.objective_defend = palette_.honey;
    hud.objective_destroy = palette_.danger;
    hud.graph_series = palette_.honey;

    hud.graph.desired_size = FVector2f{320.0f, 180.0f};
    hud.graph.label_font = hud.caption_text.Font;
    hud.graph.label_color = hud.caption_text.ColorAndOpacity.GetSpecifiedColor();
    hud.graph.axis_color = hud.secondary_text.ColorAndOpacity.GetSpecifiedColor();
    hud.graph.grid_color = hud.graph.axis_color.CopyWithNewOpacity(0.25f);
    hud.graph.background_color = palette_.surface_low.CopyWithNewOpacity(0.88f);
    hud.graph.plot_color = palette_.control.CopyWithNewOpacity(0.70f);
    hud.graph.empty_text = NSLOCTEXT("GameHud", "NoGraphData", "NO TELEMETRY");
    return compiled;
}
}
