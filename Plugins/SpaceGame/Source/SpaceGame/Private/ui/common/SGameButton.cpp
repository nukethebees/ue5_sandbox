#include "SpaceGame/ui/common/SGameButton.h"

#include "Framework/Application/SlateApplication.h"

#include <Styling/CoreStyle.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Layout/SBox.h>
#include <Widgets/Text/STextBlock.h>

namespace ml::ioj {
namespace {
auto fallback_button_style() -> FGameButtonPresentationStyle const& {
    static FGameButtonPresentationStyle const style{[] {
        FGameButtonPresentationStyle result;
        result.normal = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
        result.selected = result.normal;
        result.normal_text = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
        result.normal_hovered_text = result.normal_text;
        result.selected_text = result.normal_text;
        result.selected_hovered_text = result.normal_text;
        result.disabled_text = result.normal_text;
        result.minimum_size = FVector2f{0.0f, 32.0f};
        return result;
    }()};
    return style;
}
}

void SGameButton::Construct(FArguments const& args) {
    style_ = args._Style != nullptr ? args._Style : &fallback_button_style();
    selected_ = args._Selected;

    SAssignNew(button_, SButton)
        .ButtonStyle(selected_ ? &style_->selected : &style_->normal)
        .ContentPadding(style_->custom_padding)
        .IsEnabled(args._Enabled)
        .ToolTipText(args._ToolTipText)
        .OnClicked(args._OnClicked)
            [SNew(SBox)
                 .MinDesiredWidth(style_->minimum_size.X)
                 .MinDesiredHeight(style_->minimum_size.Y)
                 .MaxDesiredWidth(style_->maximum_size.X > 0.0f ? style_->maximum_size.X
                                                                : TOptional<float>{})
                 .MaxDesiredHeight(
                     style_->maximum_size.Y > 0.0f
                         ? style_->maximum_size.Y
                         : TOptional<float>{})[SNew(STextBlock)
                                                   .Text(args._Text)
                                                   .TextStyle(&style_->normal_text)
                                                   .ColorAndOpacity(this, &SGameButton::text_colour)
                                                   .Justification(ETextJustify::Center)]];

    ChildSlot[button_.ToSharedRef()];
}

void SGameButton::set_selected(bool const selected) {
    if (selected_ == selected) {
        return;
    }
    selected_ = selected;
    if (button_.IsValid()) {
        button_->SetButtonStyle(selected_ ? &style_->selected : &style_->normal);
    }
}

void SGameButton::focus() {
    if (button_.IsValid()) {
        FSlateApplication::Get().SetKeyboardFocus(button_, EFocusCause::SetDirectly);
    }
}

auto SGameButton::has_focus() const -> bool {
    return button_.IsValid() && (button_->HasKeyboardFocus() || button_->HasFocusedDescendants());
}

auto SGameButton::text_colour() const -> FSlateColor {
    if (!button_.IsValid() || !button_->IsEnabled()) {
        return style_->disabled_text.ColorAndOpacity;
    }
    if (selected_) {
        return button_->IsHovered() ? style_->selected_hovered_text.ColorAndOpacity
                                    : style_->selected_text.ColorAndOpacity;
    }
    return button_->IsHovered() ? style_->normal_hovered_text.ColorAndOpacity
                                : style_->normal_text.ColorAndOpacity;
}

} // namespace ml::ioj
