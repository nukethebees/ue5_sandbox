#include "SpaceGame/ui/style/SpaceGameUiTheme.h"

#include "SpaceGame/support/logging/SandboxLogCategories.h"
#include "SpaceGame/ui/common/MenuButtonWidget.h"

namespace ml::ioj {
namespace {
auto compile_text_style(TSubclassOf<UCommonTextStyle> const style_class, FTextBlockStyle& result)
    -> bool {
    auto const* const style{style_class ? style_class->GetDefaultObject<UCommonTextStyle>()
                                        : nullptr};
    if (!IsValid(style)) {
        return false;
    }

    style->ToTextBlockStyle(result);
    return true;
}

auto compile_button_style(UCommonButtonStyle const& source, FGameButtonPresentationStyle& result)
    -> bool {
    auto const& normal{source.bSingleMaterial ? source.SingleMaterialBrush : source.NormalBase};
    auto const& hovered{source.bSingleMaterial ? source.SingleMaterialBrush : source.NormalHovered};
    auto const& pressed{source.bSingleMaterial ? source.SingleMaterialBrush : source.NormalPressed};
    auto const& selected{source.bSingleMaterial ? source.SingleMaterialBrush : source.SelectedBase};
    auto const& selected_hovered{source.bSingleMaterial ? source.SingleMaterialBrush
                                                        : source.SelectedHovered};
    auto const& selected_pressed{source.bSingleMaterial ? source.SingleMaterialBrush
                                                        : source.SelectedPressed};

    result.normal.SetNormal(normal)
        .SetHovered(hovered)
        .SetPressed(pressed)
        .SetDisabled(source.Disabled)
        .SetNormalPadding(source.ButtonPadding)
        .SetPressedPadding(source.ButtonPadding);
    result.selected.SetNormal(selected)
        .SetHovered(selected_hovered)
        .SetPressed(selected_pressed)
        .SetDisabled(source.Disabled)
        .SetNormalPadding(source.ButtonPadding)
        .SetPressedPadding(source.ButtonPadding);
    result.custom_padding = source.CustomPadding;
    result.minimum_size =
        FVector2f{static_cast<float>(source.MinWidth), static_cast<float>(source.MinHeight)};
    result.maximum_size =
        FVector2f{static_cast<float>(source.MaxWidth), static_cast<float>(source.MaxHeight)};

    return compile_text_style(source.NormalTextStyle, result.normal_text) &&
           compile_text_style(source.NormalHoveredTextStyle, result.normal_hovered_text) &&
           compile_text_style(source.SelectedTextStyle, result.selected_text) &&
           compile_text_style(source.SelectedHoveredTextStyle, result.selected_hovered_text) &&
           compile_text_style(source.DisabledTextStyle, result.disabled_text);
}
}

auto FGameTextStyleClasses::get(EGameTextStyle const role) const -> TSubclassOf<UCommonTextStyle> {
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
    return nullptr;
}

auto FGameButtonStyleClasses::get(EGameButtonStyle const role) const
    -> TSubclassOf<UCommonButtonStyle> {
    switch (role) {
        case EGameButtonStyle::Primary:
            return primary;
        case EGameButtonStyle::Secondary:
            return secondary;
    }

    checkNoEntry();
    return nullptr;
}

USpaceGameUiTheme::USpaceGameUiTheme() {
    auto const default_text_style{UMenuTextStyle::StaticClass()};
    text_styles_.body = default_text_style;
    text_styles_.body_secondary = default_text_style;
    text_styles_.caption = default_text_style;
    text_styles_.heading_1 = default_text_style;
    text_styles_.heading_2 = default_text_style;
    text_styles_.heading_3 = default_text_style;
    text_styles_.warning = default_text_style;
    text_styles_.hud_primary = default_text_style;
    text_styles_.hud_secondary = default_text_style;

    auto const default_button_style{UMenuButtonStyle::StaticClass()};
    button_styles_.primary = default_button_style;
    button_styles_.secondary = default_button_style;
}

auto USpaceGameUiTheme::compile(FGameUiStyle& result) const -> bool {
    FGameUiStyle compiled;
    for (int32 index{}; index < TEnumTraits<EGameTextStyle>::count; ++index) {
        auto const role{static_cast<EGameTextStyle>(index)};
        auto const style_class{text_styles_.get(role)};
        if (!compile_text_style(style_class, compiled.text_styles_[role])) {
            UE_LOG(LogSandboxUI,
                   Error,
                   TEXT("USpaceGameUiTheme::compile: Text style '%s' is not configured."),
                   LexToString(role));
            return false;
        }
    }

    for (int32 index{}; index < TEnumTraits<EGameButtonStyle>::count; ++index) {
        auto const role{static_cast<EGameButtonStyle>(index)};
        auto const style_class{button_styles_.get(role)};
        auto const* const style{style_class ? style_class->GetDefaultObject<UCommonButtonStyle>()
                                            : nullptr};
        if (!IsValid(style) || !compile_button_style(*style, compiled.button_styles_[role])) {
            UE_LOG(LogSandboxUI,
                   Error,
                   TEXT("USpaceGameUiTheme::compile: Button style '%s' is incomplete."),
                   LexToString(role));
            return false;
        }
    }

    compiled.panel_ = FGamePanelStyle{.background = panel_background_, .padding = panel_padding_};
    compiled.health_bar_ = health_bar_;
    result = MoveTemp(compiled);
    return true;
}

auto USpaceGameUiTheme::get_common_text_style(EGameTextStyle const role) const
    -> TSubclassOf<UCommonTextStyle> {
    return text_styles_.get(role);
}

auto USpaceGameUiTheme::get_common_button_style(EGameButtonStyle const role) const
    -> TSubclassOf<UCommonButtonStyle> {
    return button_styles_.get(role);
}

void USpaceGameUiTheme::set_common_text_style(EGameTextStyle const role,
                                              TSubclassOf<UCommonTextStyle> const style) {
    switch (role) {
        case EGameTextStyle::Body:
            text_styles_.body = style;
            return;
        case EGameTextStyle::BodySecondary:
            text_styles_.body_secondary = style;
            return;
        case EGameTextStyle::Caption:
            text_styles_.caption = style;
            return;
        case EGameTextStyle::Heading1:
            text_styles_.heading_1 = style;
            return;
        case EGameTextStyle::Heading2:
            text_styles_.heading_2 = style;
            return;
        case EGameTextStyle::Heading3:
            text_styles_.heading_3 = style;
            return;
        case EGameTextStyle::Warning:
            text_styles_.warning = style;
            return;
        case EGameTextStyle::HudPrimary:
            text_styles_.hud_primary = style;
            return;
        case EGameTextStyle::HudSecondary:
            text_styles_.hud_secondary = style;
            return;
    }

    checkNoEntry();
}

void USpaceGameUiTheme::set_common_button_style(EGameButtonStyle const role,
                                                TSubclassOf<UCommonButtonStyle> const style) {
    switch (role) {
        case EGameButtonStyle::Primary:
            button_styles_.primary = style;
            return;
        case EGameButtonStyle::Secondary:
            button_styles_.secondary = style;
            return;
    }

    checkNoEntry();
}
}
