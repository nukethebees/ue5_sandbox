#include "SpaceGamePresentation/ui/common/MenuButtonWidget.h"

#include "SpaceGamePresentation/ui/style/SpaceGameUiTheme.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>
#include <SpaceGamePresentation/ui/style/GameUiStyleSubsystem.h>

#include <Components/Border.h>
#include <Components/Button.h>
#include <Components/SizeBox.h>
#include <Components/TextBlock.h>
#include <Engine/GameInstance.h>

namespace ioj {
auto UMenuButtonWidget::focus_target() const -> UWidget* {
    return button;
}
void UMenuButtonWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();
    button->OnClicked.AddDynamic(this, &ThisClass::handle_click);
    button->OnPressed.AddDynamic(this, &ThisClass::handle_press);
    button->OnReleased.AddDynamic(this, &ThisClass::update_visual_style);
    button->OnHovered.AddDynamic(this, &ThisClass::update_visual_style);
    button->OnUnhovered.AddDynamic(this, &ThisClass::update_visual_style);
}
void UMenuButtonWidget::handle_click() {
    clicked.Broadcast();
}
void UMenuButtonWidget::handle_press() {
    audio_.play_button_pressed();
    update_visual_style();
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

void UMenuButtonWidget::set_audio(FGameAudioFacade audio) {
    audio_ = MoveTemp(audio);
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

void UMenuButtonWidget::NativeOnAddedToFocusPath(FFocusEvent const& focus_event) {
    Super::NativeOnAddedToFocusPath(focus_event);
    update_visual_style();
}

void UMenuButtonWidget::NativeOnRemovedFromFocusPath(FFocusEvent const& focus_event) {
    Super::NativeOnRemovedFromFocusPath(focus_event);
    update_visual_style();
}

auto UMenuButtonWidget::resolve_style() const -> FGameButtonPresentationStyle {
    if (has_style_override_) {
        return style_override_;
    }

    auto const* const game_instance{GetGameInstance()};
    auto const* const subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<UGameUiStyleSubsystem>() : nullptr};
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

    auto const interaction_enabled{GetIsEnabled()};
    auto const hovered{IsHovered()};
    auto const focused{button->HasAnyUserFocus()};
    auto const pressed{button->IsPressed()};
    auto const& button_style{resolved_style_.normal};

    auto const* brush{&button_style.Normal};
    if (!interaction_enabled) {
        brush = &button_style.Disabled;
    } else if (pressed) {
        brush = &button_style.Pressed;
    } else if (hovered || focused) {
        brush = &button_style.Hovered;
    }
    background->SetBrush(*brush);
    background->SetPadding((pressed ? button_style.PressedPadding : button_style.NormalPadding) +
                           resolved_style_.custom_padding);

    auto const* text_style{&resolved_style_.normal_text};
    if (!interaction_enabled) {
        text_style = &resolved_style_.disabled_text;
    } else if (hovered || focused) {
        text_style = &resolved_style_.normal_hovered_text;
    }
    apply_text_style(*label_text, *text_style);

    button_size->SetMinDesiredWidth(resolved_style_.minimum_size.X);
    button_size->SetMinDesiredHeight(resolved_style_.minimum_size.Y);
    if (resolved_style_.maximum_size.X > 0.f) {
        button_size->SetMaxDesiredWidth(resolved_style_.maximum_size.X);
    }
    if (resolved_style_.maximum_size.Y > 0.f) {
        button_size->SetMaxDesiredHeight(resolved_style_.maximum_size.Y);
    }
}
}
