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

struct FButtonPalette {
    FLinearColor normal{};
    FLinearColor hovered{};
    FLinearColor pressed{};
    FLinearColor selected{};
    FLinearColor selected_hovered{};
    FLinearColor selected_pressed{};
    FLinearColor disabled{};
};

auto make_button_state_style(FLinearColor const& normal,
                             FLinearColor const& hovered,
                             FLinearColor const& pressed,
                             FLinearColor const& disabled) -> FButtonStyle {
    return FButtonStyle{}
        .SetNormal(make_brush(normal))
        .SetHovered(make_brush(hovered))
        .SetPressed(make_brush(pressed))
        .SetDisabled(make_brush(disabled))
        .SetNormalPadding(FMargin{10.f, 5.f})
        .SetPressedPadding(FMargin{10.f, 5.f});
}

auto make_button_style(FButtonPalette const& palette,
                       EGameTextStyle const normal_text = EGameTextStyle::Body)
    -> FGameButtonStyleDefinition {
    FGameButtonStyleDefinition result;
    result.normal =
        make_button_state_style(palette.normal, palette.hovered, palette.pressed, palette.disabled);
    result.selected = make_button_state_style(
        palette.selected, palette.selected_hovered, palette.selected_pressed, palette.disabled);
    result.normal_text = normal_text;
    result.minimum_size = FVector2f{0.f, 28.f};
    return result;
}

auto compile_button_style(FGameButtonStyleDefinition const& source,
                          FGameTextStyles const& text_styles) -> FGameButtonPresentationStyle {
    return FGameButtonPresentationStyle{
        .normal = source.normal,
        .selected = source.selected,
        .normal_text = text_styles.get(source.normal_text),
        .normal_hovered_text = text_styles.get(source.normal_hovered_text),
        .selected_text = text_styles.get(source.selected_text),
        .selected_hovered_text = text_styles.get(source.selected_hovered_text),
        .disabled_text = text_styles.get(source.disabled_text),
        .custom_padding = source.custom_padding,
        .minimum_size = source.minimum_size,
        .maximum_size = source.maximum_size,
    };
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
        case EGameTextStyle::Disabled:
            return disabled;
    }

    checkNoEntry();
    return body;
}

auto FGameButtonStyles::get(EGameButtonStyle const role) const
    -> FGameButtonStyleDefinition const& {
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
    text_styles_.disabled =
        make_text_style(TEXT("Regular"), 14, FLinearColor{0.5f, 0.52f, 0.55f, 1.f});

    button_styles_.primary = make_button_style(FButtonPalette{
        .normal = FLinearColor{0.12f, 0.14f, 0.16f, 1.f},
        .hovered = FLinearColor{0.16f, 0.38f, 0.55f, 1.f},
        .pressed = FLinearColor{0.08f, 0.3f, 0.65f, 1.f},
        .selected = FLinearColor{0.08f, 0.3f, 0.65f, 1.f},
        .selected_hovered = FLinearColor{0.12f, 0.4f, 0.78f, 1.f},
        .selected_pressed = FLinearColor{0.06f, 0.24f, 0.52f, 1.f},
        .disabled = FLinearColor{0.08f, 0.09f, 0.1f, 0.6f},
    });
    button_styles_.secondary = make_button_style(
        FButtonPalette{
            .normal = FLinearColor{0.1f, 0.11f, 0.12f, 1.f},
            .hovered = FLinearColor{0.18f, 0.2f, 0.22f, 1.f},
            .pressed = FLinearColor{0.07f, 0.08f, 0.09f, 1.f},
            .selected = FLinearColor{0.14f, 0.18f, 0.22f, 1.f},
            .selected_hovered = FLinearColor{0.22f, 0.26f, 0.3f, 1.f},
            .selected_pressed = FLinearColor{0.1f, 0.14f, 0.18f, 1.f},
            .disabled = FLinearColor{0.08f, 0.09f, 0.1f, 0.6f},
        },
        EGameTextStyle::BodySecondary);
}

auto USpaceGameUiTheme::compile() const -> FGameUiStyle {
    FGameUiStyle compiled;
    for (int32 index{}; index < TEnumTraits<EGameTextStyle>::count; ++index) {
        auto const role{static_cast<EGameTextStyle>(index)};
        compiled.text_styles_[role] = text_styles_.get(role);
    }

    for (int32 index{}; index < TEnumTraits<EGameButtonStyle>::count; ++index) {
        auto const role{static_cast<EGameButtonStyle>(index)};
        compiled.button_styles_[role] =
            compile_button_style(button_styles_.get(role), text_styles_);
    }

    compiled.panel_ = FGamePanelStyle{.background = panel_background_, .padding = panel_padding_};
    compiled.health_bar_ = health_bar_;
    return compiled;
}
}
