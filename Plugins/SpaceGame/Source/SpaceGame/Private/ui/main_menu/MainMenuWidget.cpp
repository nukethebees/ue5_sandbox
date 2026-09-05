#include "SpaceGame/ui/main_menu/MainMenuWidget.h"

#include "SpaceGame/support/logging/SandboxLogCategories.h"
#include "SpaceGame/ui/common/MenuButtonWidget.h"
#include "SpaceGame/ui/main_menu/OptionsWidget.h"
#include "SpaceGame/ui/save_game/SaveGameViewerWidget.h"

#include <Components/Button.h>
#include <Components/VerticalBox.h>
#include <Components/WidgetSwitcher.h>
#include <Kismet/KismetSystemLibrary.h>

namespace ml::ioj {
void UMainMenuWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    if (!IsValid(play_button) || !IsValid(save_games_button) || !IsValid(options_button) ||
        !IsValid(quit_button) || !IsValid(save_games_page) || !IsValid(save_game_viewer) ||
        !IsValid(save_games_back_button) || !IsValid(options_widget)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UMainMenuWidget::NativeOnInitialized: One or more bound widgets are "
                    "invalid."));
        return;
    }

    play_button->OnClicked().AddUObject(this, &ThisClass::handle_play);
    save_games_button->OnClicked().AddUObject(this, &ThisClass::handle_save_games);
    options_button->OnClicked().AddUObject(this, &ThisClass::handle_options);
    quit_button->OnClicked().AddUObject(this, &ThisClass::handle_quit);
    save_games_back_button->OnClicked.AddDynamic(this, &ThisClass::return_from_save_games);
    options_widget->back_requested.AddUObject(this, &ThisClass::return_from_options);
    main_focus_target_ = play_button;
}

auto UMainMenuWidget::NativeGetDesiredFocusTarget() const -> UWidget* {
    switch (active_page_) {
        case EMainMenuPage::Main: {
            return IsValid(main_focus_target_) ? main_focus_target_.Get() : play_button;
        }
        case EMainMenuPage::SaveGames: {
            return IsValid(save_game_viewer) ? save_game_viewer->get_focus_target() : nullptr;
        }
        case EMainMenuPage::Options: {
            return IsValid(options_widget) ? options_widget->get_focus_target() : nullptr;
        }
    }
    return nullptr;
}

auto UMainMenuWidget::NativeOnHandleBackAction() -> bool {
    switch (active_page_) {
        case EMainMenuPage::SaveGames: {
            return_from_save_games();
            break;
        }
        case EMainMenuPage::Options: {
            options_widget->request_back();
            break;
        }
        case EMainMenuPage::Main: {
            break;
        }
    }
    return true;
}

void UMainMenuWidget::handle_play() {
    main_focus_target_ = play_button;
    level_select_requested.Broadcast();
}

void UMainMenuWidget::handle_save_games() {
    set_active_page(EMainMenuPage::SaveGames);
}

void UMainMenuWidget::handle_options() {
    options_widget->prepare_for_open();
    set_active_page(EMainMenuPage::Options);
}

void UMainMenuWidget::handle_quit() {
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UMainMenuWidget::return_from_save_games() {
    main_focus_target_ = save_games_button;
    set_active_page(EMainMenuPage::Main);
}

void UMainMenuWidget::return_from_options() {
    main_focus_target_ = options_button;
    set_active_page(EMainMenuPage::Main);
}

void UMainMenuWidget::set_active_page(EMainMenuPage const page) {
    if (!IsValid(page_switcher) || !IsValid(main_page) || !IsValid(save_games_page) ||
        !IsValid(options_widget)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UMainMenuWidget::set_active_page: One or more bound widgets are invalid."));
        return;
    }

    UWidget* active_widget{nullptr};
    switch (page) {
        case EMainMenuPage::Main: {
            active_widget = main_page;
            break;
        }
        case EMainMenuPage::SaveGames: {
            active_widget = save_games_page;
            break;
        }
        case EMainMenuPage::Options: {
            active_widget = options_widget;
            break;
        }
        default: {
            UE_LOG(LogSandboxUI,
                   Error,
                   TEXT("UMainMenuWidget::set_active_page: Unhandled page value %d."),
                   static_cast<int32>(page));
            return;
        }
    }

    active_page_ = page;
    page_switcher->SetActiveWidget(active_widget);
    RequestRefreshFocus();
}
}
