#include "SpaceGame/ui/PauseMenuWidget.h"

#include "SpaceGamePresentation/ui/common/MenuButtonWidget.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"
#include "SpaceGamePresentation/ui/style/SpaceGameUiTheme.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"
#include <SpaceGame/UnrealEndpoint.h>
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
    options_widget->show_page(unreal_endpoint().menu().page);
    refresh_navigation();
}

auto UPauseMenuWidget::focus_target() const -> UWidget* {
    return resume_button->focus_target();
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
    unreal_endpoint().pause_interaction(event);
}

void UPauseMenuWidget::present(host::MenuMessage const& message) {
    options_widget->show_page(message.state.page);
    refresh_navigation();
    if (!message.update) {
        return;
    }
    auto const& update{*message.update};
    if (update.target.kind == MenuTarget::Kind::PauseItem) {
        options_widget->refresh_view();
        button_for(update.target.pause_item())->focus_target()->SetKeyboardFocus();
    } else {
        options_widget->present(update);
    }
}

void UPauseMenuWidget::refresh_navigation() {
    auto const enabled{!unreal_endpoint().menu().navigation_blocked()};
    for (auto* const button : {resume_button,
                               audio_button,
                               controls_button,
                               return_to_level_select_button,
                               quit_button}) {
        if (IsValid(button)) {
            auto const page{unreal_endpoint().menu().page};
            button->SetIsEnabled(enabled &&
                                 !(button == audio_button && page == MainMenuItem::Audio) &&
                                 !(button == controls_button && page == MainMenuItem::Controls));
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
