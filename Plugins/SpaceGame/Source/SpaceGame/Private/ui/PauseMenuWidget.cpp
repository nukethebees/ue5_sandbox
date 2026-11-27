#include "SpaceGame/ui/PauseMenuWidget.h"

#include "SpaceGamePresentation/ui/common/MenuButtonWidget.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"
#include "SpaceGamePresentation/ui/style/SpaceGameUiTheme.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"
#include <SpaceGame/UnrealMessages.h>
#include <SpaceGamePresentation/audio/GameAudio.h>
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>
#include <SpaceGamePresentation/ui/style/GameUiStyleSubsystem.h>

#include <SandboxCoreEngine/strings.h>

#include <Components/TextBlock.h>
#include <Engine/GameInstance.h>
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

    using enum host::PauseMenuItem;
    for (auto item : {Resume, Audio, Controls, LevelSelect, Quit}) {
        auto* button{button_for(item)};
        button->clicked.AddUObject(
            this,
            &ThisClass::send_event,
            host::PauseWidgetEvent{host::PauseWidgetEventKind::Activate, item});
        button->focus_received.AddUObject(
            this,
            &ThisClass::send_event,
            host::PauseWidgetEvent{host::PauseWidgetEventKind::Select, item});
    }
    options_widget->content_focused.AddUObject(
        this,
        &ThisClass::send_event,
        host::PauseWidgetEvent{host::PauseWidgetEventKind::ContentFocused});
    options_widget->modal_state_changed.AddUObject(this,
                                                   &ThisClass::handle_options_modal_state_changed);

    auto* const game_instance{GetGameInstance()};
    auto* const game{IsValid(game_instance) ? game_instance->GetSubsystem<UGameAudio>() : nullptr};
    auto const audio{IsValid(game) ? game->facade() : FGameAudioFacade{}};
    for (auto* const button : {resume_button,
                               audio_button,
                               controls_button,
                               return_to_level_select_button,
                               quit_button}) {
        button->set_audio(audio);
    }
}

void UPauseMenuWidget::NativeConstruct() {
    Super::NativeConstruct();
    apply_ui_style();
}

void UPauseMenuWidget::prepare_for_open() {
    options_modal_visible_ = false;
    options_widget->select_page(EMenuPage::Audio, {});
    set_navigation_enabled(true);
}

auto UPauseMenuWidget::focus_target() const -> UWidget* {
    return resume_button->focus_target();
}

auto UPauseMenuWidget::handle_back() -> bool {
    if (options_modal_visible_) {
        options_widget->request_back();
    } else {
        send_event({host::PauseWidgetEventKind::Activate, host::PauseMenuItem::Resume});
    }
    return true;
}

auto UPauseMenuWidget::button_for(host::PauseMenuItem item) const -> UMenuButtonWidget* {
    using enum host::PauseMenuItem;
    switch (item) {
        case Resume:
            return resume_button;
        case Audio:
            return audio_button;
        case Controls:
            return controls_button;
        case LevelSelect:
            return return_to_level_select_button;
        case Quit:
            return quit_button;
    }
    checkNoEntry();
    return nullptr;
}

void UPauseMenuWidget::send_event(host::PauseWidgetEvent event) {
    queue_input_action(event);
}

void UPauseMenuWidget::present(host::PauseCommand command) {
    using enum host::PauseCommandKind;
    switch (command.kind) {
        case Open:
        case host::PauseCommandKind::Select:
            button_for(command.item)->focus_target()->SetKeyboardFocus();
            break;
        case CheckLeave:
            options_widget->request_leave(
                FSimpleDelegate::CreateUObject(
                    this,
                    &ThisClass::send_event,
                    host::PauseWidgetEvent{host::PauseWidgetEventKind::LeaveAccepted,
                                           command.item}),
                FSimpleDelegate::CreateUObject(
                    this,
                    &ThisClass::send_event,
                    host::PauseWidgetEvent{host::PauseWidgetEventKind::LeaveCancelled,
                                           command.item}));
            break;
        case ShowPage:
            options_widget->select_page(
                command.item == host::PauseMenuItem::Audio ? EMenuPage::Audio : EMenuPage::Controls,
                FSimpleDelegate::CreateUObject(options_widget, &UOptionsWidget::focus_content));
            set_navigation_enabled(!options_modal_visible_);
            break;
        case Finish:
            if (command.item == host::PauseMenuItem::Resume) {
                close();
            } else if (command.item == host::PauseMenuItem::LevelSelect) {
                return_to_level_select_requested.Broadcast();
            } else {
                check(command.item == host::PauseMenuItem::Quit);
                quit_requested.Broadcast();
            }
            break;
    }
}

void UPauseMenuWidget::handle_options_modal_state_changed(bool const visible) {
    options_modal_visible_ = visible;
    send_event({visible ? host::PauseWidgetEventKind::ModalOpened
                        : host::PauseWidgetEventKind::ModalClosed});
    set_navigation_enabled(!visible);
}

void UPauseMenuWidget::set_navigation_enabled(bool const enabled) {
    for (auto* const button : {resume_button,
                               audio_button,
                               controls_button,
                               return_to_level_select_button,
                               quit_button}) {
        if (IsValid(button)) {
            auto const page{options_widget->page()};
            button->SetIsEnabled(enabled && !(button == audio_button && page == EMenuPage::Audio) &&
                                 !(button == controls_button && page == EMenuPage::Controls));
        }
    }
}

/* **************************************** */
// View state and presentation
/* **************************************** */

void UPauseMenuWidget::apply_ui_style() {
    FGameUiStyle style;
    auto const* const game_instance{GetGameInstance()};
    auto const* const subsystem{UGameInstance::GetSubsystem<UGameUiStyleSubsystem>(game_instance)};
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
