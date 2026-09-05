#include "SpaceGame/ui/style/SpaceGameUiTheme.h"

#include <Styling/CoreStyle.h>

namespace ml::ioj {
namespace {
auto make_text_style(TCHAR const* const typeface, int32 const size, FLinearColor const& colour)
    -> FTextBlockStyle {
    return FTextBlockStyle{}
        .SetFont(FCoreStyle::GetDefaultFontStyle(typeface, size))
        .SetColorAndOpacity(FSlateColor{colour});
}

auto make_brush(FLinearColor const& colour) -> FSlateBrush {
    FSlateBrush brush;
    brush.DrawAs = ESlateBrushDrawType::Box;
    brush.TintColor = FSlateColor{colour};
    return brush;
}

auto make_button_style(FSlateBrush const& normal,
                       FSlateBrush const& hovered,
                       FSlateBrush const& pressed,
                       FSlateBrush const& selected,
                       FSlateBrush const& selected_hovered,
                       FSlateBrush const& selected_pressed,
                       FSlateBrush const& disabled,
                       FTextBlockStyle const& normal_text,
                       FTextBlockStyle const& hovered_text,
                       FTextBlockStyle const& selected_text,
                       FTextBlockStyle const& selected_hovered_text,
                       FTextBlockStyle const& disabled_text) -> FGameButtonPresentationStyle {
    FGameButtonPresentationStyle result;
    result.normal.SetNormal(normal)
        .SetHovered(hovered)
        .SetPressed(pressed)
        .SetDisabled(disabled)
        .SetNormalPadding(FMargin{10.f, 5.f})
        .SetPressedPadding(FMargin{10.f, 5.f});
    result.selected.SetNormal(selected)
        .SetHovered(selected_hovered)
        .SetPressed(selected_pressed)
        .SetDisabled(disabled)
        .SetNormalPadding(FMargin{10.f, 5.f})
        .SetPressedPadding(FMargin{10.f, 5.f});
    result.normal_text = normal_text;
    result.normal_hovered_text = hovered_text;
    result.selected_text = selected_text;
    result.selected_hovered_text = selected_hovered_text;
    result.disabled_text = disabled_text;
    result.minimum_size = FVector2f{0.f, 28.f};
    return result;
}
}

auto FGameTextStyles::get(EGameTextStyle const role) const -> FTextBlockStyle const& {
    switch (role) {
        case EGameTextStyle::Body:
            return body;
        case EGameTextStyle::BodySecondary:
            return body_secondary;
        case EGameTextStyle::Caption:
            return caption;
        case EGameTextStyle::Heading1:
            return heading_1;
        case EGameTextStyle::Heading2:
            return heading_2;
        case EGameTextStyle::Heading3:
            return heading_3;
        case EGameTextStyle::Warning:
            return warning;
        case EGameTextStyle::HudPrimary:
            return hud_primary;
        case EGameTextStyle::HudSecondary:
            return hud_secondary;
    }

    checkNoEntry();
    return body;
}

auto FGameButtonStyles::get(EGameButtonStyle const role) const
    -> FGameButtonPresentationStyle const& {
    switch (role) {
        case EGameButtonStyle::Primary:
            return primary;
        case EGameButtonStyle::Secondary:
            return secondary;
    }

    checkNoEntry();
    return primary;
}

USpaceGameUiTheme::USpaceGameUiTheme() {
    text_styles_.body = make_text_style(TEXT("Regular"), 14, FLinearColor::White);
    text_styles_.body_secondary =
        make_text_style(TEXT("Regular"), 14, FLinearColor{0.72f, 0.76f, 0.82f, 1.f});
    text_styles_.caption =
        make_text_style(TEXT("Regular"), 12, FLinearColor{0.6f, 0.64f, 0.7f, 1.f});
    text_styles_.heading_1 = make_text_style(TEXT("Bold"), 28, FLinearColor::White);
    text_styles_.heading_2 = make_text_style(TEXT("Bold"), 22, FLinearColor::White);
    text_styles_.heading_3 = make_text_style(TEXT("Bold"), 18, FLinearColor::White);
    text_styles_.warning = make_text_style(TEXT("Bold"), 14, FLinearColor{1.f, 0.35f, 0.15f, 1.f});
    text_styles_.hud_primary = make_text_style(TEXT("Bold"), 18, FLinearColor::White);
    text_styles_.hud_secondary =
        make_text_style(TEXT("Regular"), 14, FLinearColor{0.72f, 0.76f, 0.82f, 1.f});

    auto const disabled_text{
        make_text_style(TEXT("Regular"), 14, FLinearColor{0.5f, 0.52f, 0.55f, 1.f})};
    button_styles_.primary = make_button_style(make_brush(FLinearColor{0.12f, 0.14f, 0.16f, 1.f}),
                                               make_brush(FLinearColor{0.16f, 0.38f, 0.55f, 1.f}),
                                               make_brush(FLinearColor{0.08f, 0.3f, 0.65f, 1.f}),
                                               make_brush(FLinearColor{0.08f, 0.3f, 0.65f, 1.f}),
                                               make_brush(FLinearColor{0.12f, 0.4f, 0.78f, 1.f}),
                                               make_brush(FLinearColor{0.06f, 0.24f, 0.52f, 1.f}),
                                               make_brush(FLinearColor{0.08f, 0.09f, 0.1f, 0.6f}),
                                               text_styles_.body,
                                               text_styles_.body,
                                               text_styles_.body,
                                               text_styles_.body,
                                               disabled_text);
    button_styles_.secondary = make_button_style(make_brush(FLinearColor{0.1f, 0.11f, 0.12f, 1.f}),
                                                 make_brush(FLinearColor{0.18f, 0.2f, 0.22f, 1.f}),
                                                 make_brush(FLinearColor{0.07f, 0.08f, 0.09f, 1.f}),
                                                 make_brush(FLinearColor{0.14f, 0.18f, 0.22f, 1.f}),
                                                 make_brush(FLinearColor{0.22f, 0.26f, 0.3f, 1.f}),
                                                 make_brush(FLinearColor{0.1f, 0.14f, 0.18f, 1.f}),
                                                 make_brush(FLinearColor{0.08f, 0.09f, 0.1f, 0.6f}),
                                                 text_styles_.body_secondary,
                                                 text_styles_.body,
                                                 text_styles_.body,
                                                 text_styles_.body,
                                                 disabled_text);
}

auto USpaceGameUiTheme::compile() const -> FGameUiStyle {
    FGameUiStyle compiled;
    for (int32 index{}; index < TEnumTraits<EGameTextStyle>::count; ++index) {
        auto const role{static_cast<EGameTextStyle>(index)};
        compiled.text_styles_[role] = text_styles_.get(role);
    }

    for (int32 index{}; index < TEnumTraits<EGameButtonStyle>::count; ++index) {
        auto const role{static_cast<EGameButtonStyle>(index)};
        compiled.button_styles_[role] = button_styles_.get(role);
    }

    compiled.panel_ = FGamePanelStyle{.background = panel_background_, .padding = panel_padding_};
    compiled.health_bar_ = health_bar_;
    return compiled;
}
}
