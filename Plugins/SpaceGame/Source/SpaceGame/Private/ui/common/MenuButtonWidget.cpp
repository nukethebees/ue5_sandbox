#include "SpaceGame/ui/common/MenuButtonWidget.h"

#include "SpaceGame/support/logging/SandboxLogCategories.h"
#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGame/ui/style/SpaceGameUiTheme.h"

#include <Components/Border.h>
#include <Engine/GameInstance.h>
#include <Styling/CoreStyle.h>

namespace ml::ioj {
namespace {
auto make_no_draw_brush() -> FSlateBrush {
    FSlateBrush brush;
    brush.DrawAs = ESlateBrushDrawType::NoDrawType;
    return brush;
}

void apply_text_style(UCommonTextBlock& text, FTextBlockStyle const& style) {
    text.SetFont(style.Font);
    text.SetColorAndOpacity(style.ColorAndOpacity);
    text.SetShadowOffset(style.ShadowOffset);
    text.SetShadowColorAndOpacity(style.ShadowColorAndOpacity);
    text.SetStrikeBrush(style.StrikeBrush);
    text.SetTextTransformPolicy(style.TransformPolicy);
    text.SetTextOverflowPolicy(style.OverflowPolicy);
}
}

UMenuTextStyle::UMenuTextStyle() {
    Font = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14);
    Color = FLinearColor::White;
}

UMenuButtonStyle::UMenuButtonStyle() {
    bSingleMaterial = false;
    NormalBase = make_no_draw_brush();
    NormalHovered = make_no_draw_brush();
    NormalPressed = make_no_draw_brush();
    SelectedBase = make_no_draw_brush();
    SelectedHovered = make_no_draw_brush();
    SelectedPressed = make_no_draw_brush();
    Disabled = make_no_draw_brush();
    ButtonPadding = FMargin{};
    CustomPadding = FMargin{};
    MinWidth = 0;
    MinHeight = 0;
    MaxWidth = 0;
    MaxHeight = 0;
    NormalTextStyle = UMenuTextStyle::StaticClass();
    NormalHoveredTextStyle = UMenuTextStyle::StaticClass();
    SelectedTextStyle = UMenuTextStyle::StaticClass();
    SelectedHoveredTextStyle = UMenuTextStyle::StaticClass();
    DisabledTextStyle = UMenuTextStyle::StaticClass();
}

UMenuButtonWidget::UMenuButtonWidget() {
    SetStyle(UMenuButtonStyle::StaticClass());
}

void UMenuButtonWidget::set_text(FText const& text) {
    text_ = text;
    if (IsValid(label_text)) {
        label_text->SetText(text_);
    }
}

auto UMenuButtonWidget::get_text() const -> FText {
    return text_;
}

void UMenuButtonWidget::NativePreConstruct() {
    resolved_style_ = resolve_style();
    Super::NativePreConstruct();

    if (!IsValid(background) || !IsValid(label_text)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UMenuButtonWidget::NativePreConstruct: Required widgets are invalid."));
        return;
    }

    label_text->SetText(text_);
    update_visual_style();
}

void UMenuButtonWidget::NativeOnPressed() {
    Super::NativeOnPressed();
    update_visual_style();
}

void UMenuButtonWidget::NativeOnReleased() {
    Super::NativeOnReleased();
    update_visual_style();
}

void UMenuButtonWidget::NativeOnCurrentTextStyleChanged() {
    Super::NativeOnCurrentTextStyleChanged();
    update_visual_style();
}

auto UMenuButtonWidget::resolve_style() const -> FGameButtonPresentationStyle {
    if (has_style_override_) {
        return style_override_;
    }

    auto const* const game_instance{GetGameInstance()};
    auto const* const subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<UGameSubsystem>() : nullptr};
    if (IsValid(subsystem)) {
        return subsystem->get_ui_style().button(style_role_);
    }

    auto const* const default_theme{GetDefault<USpaceGameUiTheme>()};
    check(IsValid(default_theme));
    return default_theme->compile().button(style_role_);
}

void UMenuButtonWidget::update_visual_style() {
    if (!IsValid(background) || !IsValid(label_text)) {
        return;
    }

    auto const selected{GetSelected()};
    auto const interaction_enabled{IsInteractionEnabled()};
    auto const hovered{IsHovered()};
    auto const pressed{IsPressed()};
    auto const& button_style{selected ? resolved_style_.selected : resolved_style_.normal};

    auto const* brush{&button_style.Normal};
    if (!interaction_enabled) {
        brush = &button_style.Disabled;
    } else if (pressed) {
        brush = &button_style.Pressed;
    } else if (hovered) {
        brush = &button_style.Hovered;
    }
    background->SetBrush(*brush);
    background->SetPadding((pressed ? button_style.PressedPadding : button_style.NormalPadding) +
                           resolved_style_.custom_padding);

    auto const* text_style{&resolved_style_.normal_text};
    if (!interaction_enabled) {
        text_style = &resolved_style_.disabled_text;
    } else if (selected) {
        text_style =
            hovered ? &resolved_style_.selected_hovered_text : &resolved_style_.selected_text;
    } else if (hovered) {
        text_style = &resolved_style_.normal_hovered_text;
    }
    apply_text_style(*label_text, *text_style);

    SetMinDimensions(FMath::RoundToInt(resolved_style_.minimum_size.X),
                     FMath::RoundToInt(resolved_style_.minimum_size.Y));
    SetMaxDimensions(FMath::RoundToInt(resolved_style_.maximum_size.X),
                     FMath::RoundToInt(resolved_style_.maximum_size.Y));
}
}
