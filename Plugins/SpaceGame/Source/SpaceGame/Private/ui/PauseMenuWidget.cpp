#include "SpaceGame/ui/PauseMenuWidget.h"

#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGamePresentation/ui/common/MenuButtonWidget.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"
#include "SpaceGamePresentation/ui/style/SpaceGameUiTheme.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>

#include <Components/TextBlock.h>
#include <Engine/GameInstance.h>
#include <Input/CommonUIInputTypes.h>
#include <Input/UIActionBinding.h>
#include <InputAction.h>
#include <Widgets/DeclarativeSyntaxSupport.h>

namespace ioj {
/* **************************************** */
// Widget lifecycle
/* **************************************** */
void UPauseMenuWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    if (!IsValid(resume_button) || !IsValid(audio_button) || !IsValid(controls_button) ||
        !IsValid(options_widget) || !IsValid(return_to_level_select_button) ||
        !IsValid(quit_button) || !IsValid(page_heading) || !IsValid(paused_heading)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UPauseMenuWidget::NativeOnInitialized: One or more bound widgets are "
                    "invalid."));
        return;
    }

    resume_button->OnClicked().AddUObject(this, &ThisClass::handle_resume);
    audio_button->OnClicked().AddUObject(this, &ThisClass::handle_audio);
    controls_button->OnClicked().AddUObject(this, &ThisClass::handle_controls);
    return_to_level_select_button->OnClicked().AddUObject(
        this, &ThisClass::handle_return_to_level_select);
    quit_button->OnClicked().AddUObject(this, &ThisClass::handle_quit);
    options_widget->modal_state_changed.AddUObject(this,
                                                   &ThisClass::handle_options_modal_state_changed);

    auto* const game_instance{GetGameInstance()};
    auto* const game{IsValid(game_instance) ? game_instance->GetSubsystem<UGameSubsystem>()
                                            : nullptr};
    auto const audio{IsValid(game) ? game->get_audio() : FGameAudioFacade{}};
    for (auto* const button : {resume_button,
                               audio_button,
                               controls_button,
                               return_to_level_select_button,
                               quit_button}) {
        button->set_audio(audio);
    }

    for (auto* const button : {audio_button, controls_button}) {
        button->SetIsSelectable(true);
        button->SetIsToggleable(true);
    }
}

void UPauseMenuWidget::NativeConstruct() {
    Super::NativeConstruct();
    apply_ui_style();
}

void UPauseMenuWidget::prepare_for_open(UInputAction& toggle_action) {
    toggle_action_ = &toggle_action;
    terminal_action_requested_ = false;
    options_modal_visible_ = false;
    set_navigation_enabled(true);
}

void UPauseMenuWidget::NativeOnActivated() {
    options_widget->prepare_for_open();
    show_options_tab(EOptionsTab::Audio);
    if (auto* const toggle_action{toggle_action_.Get()}; IsValid(toggle_action)) {
        FBindUIActionArgs const args{
            toggle_action,
            false,
            FSimpleDelegate::CreateUObject(this, &ThisClass::handle_toggle_action)};
        toggle_action_binding_ = RegisterUIActionBinding(args);
    }
    Super::NativeOnActivated();
}

void UPauseMenuWidget::NativeOnDeactivated() {
    if (toggle_action_binding_.IsValid()) {
        toggle_action_binding_.Unregister();
        toggle_action_binding_ = {};
    }
    Super::NativeOnDeactivated();
}

auto UPauseMenuWidget::NativeGetDesiredFocusTarget() const -> UWidget* {
    return resume_button;
}

auto UPauseMenuWidget::NativeOnHandleBackAction() -> bool {
    handle_toggle_action();
    return true;
}

/* **************************************** */
// Navigation callbacks
/* **************************************** */
void UPauseMenuWidget::handle_resume() {
    request_close(FSimpleDelegate::CreateUObject(this, &ThisClass::complete_resume));
}

void UPauseMenuWidget::handle_audio() {
    show_options_tab(EOptionsTab::Audio);
}

void UPauseMenuWidget::handle_controls() {
    show_options_tab(EOptionsTab::Controls);
}

void UPauseMenuWidget::handle_return_to_level_select() {
    request_close(
        FSimpleDelegate::CreateUObject(this, &ThisClass::complete_return_to_level_select));
}

void UPauseMenuWidget::handle_quit() {
    request_close(FSimpleDelegate::CreateUObject(this, &ThisClass::complete_quit));
}

void UPauseMenuWidget::handle_toggle_action() {
    request_close(FSimpleDelegate::CreateUObject(this, &ThisClass::complete_resume));
}

void UPauseMenuWidget::show_options_tab(EOptionsTab const tab) {
    if (terminal_action_requested_ || options_modal_visible_ || !IsValid(options_widget)) {
        return;
    }
    options_widget->select_tab(tab);
    audio_button->SetIsSelected(tab == EOptionsTab::Audio);
    controls_button->SetIsSelected(tab == EOptionsTab::Controls);
    options_widget->focus_content();
}

void UPauseMenuWidget::request_close(FSimpleDelegate continuation) {
    if (terminal_action_requested_) {
        return;
    }
    if (options_modal_visible_) {
        if (IsValid(options_widget)) {
            options_widget->request_back();
        }
        return;
    }
    if (IsValid(options_widget)) {
        options_widget->request_leave(MoveTemp(continuation));
        return;
    }
    continuation.ExecuteIfBound();
}

void UPauseMenuWidget::complete_resume() {
    DeactivateWidget();
}

void UPauseMenuWidget::complete_return_to_level_select() {
    if (!terminal_action_requested_) {
        terminal_action_requested_ = true;
        return_to_level_select_requested.Broadcast();
    }
}

void UPauseMenuWidget::complete_quit() {
    if (!terminal_action_requested_) {
        terminal_action_requested_ = true;
        quit_requested.Broadcast();
    }
}

void UPauseMenuWidget::handle_options_modal_state_changed(bool const visible) {
    options_modal_visible_ = visible;
    set_navigation_enabled(!visible);
}

void UPauseMenuWidget::set_navigation_enabled(bool const enabled) {
    for (auto* const button : {resume_button,
                               audio_button,
                               controls_button,
                               return_to_level_select_button,
                               quit_button}) {
        if (IsValid(button)) {
            button->SetIsEnabled(enabled);
        }
    }
}

/* **************************************** */
// View state and presentation
/* **************************************** */

void UPauseMenuWidget::apply_ui_style() {
    FGameUiStyle style;
    auto const* const game_instance{GetGameInstance()};
    auto const* const subsystem{UGameSubsystem::get(game_instance)};
    if (IsValid(subsystem)) {
        style = subsystem->get_ui_style();
    } else {
        auto const* const default_theme{GetDefault<USpaceGameUiTheme>()};
        check(IsValid(default_theme));
        style = default_theme->compile();
    }

    apply_text_style(*paused_heading, style.text(EGameTextStyle::Heading1));
    apply_text_style(*page_heading, style.text(EGameTextStyle::Heading1));
}

}
