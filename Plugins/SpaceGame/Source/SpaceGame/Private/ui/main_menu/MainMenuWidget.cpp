#include "SpaceGame/ui/main_menu/MainMenuWidget.h"

#include "SpaceGame/support/logging/SandboxLogCategories.h"
#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGame/ui/main_menu/LevelSelectWidget.h"
#include "SpaceGame/ui/main_menu/OptionsWidget.h"
#include "SpaceGame/ui/save_game/SaveGameViewerWidget.h"

#include <Components/Button.h>
#include <Components/VerticalBox.h>
#include <Components/WidgetSwitcher.h>
#include <Engine/GameInstance.h>
#include <Kismet/KismetSystemLibrary.h>

namespace ml::ioj {
void UMainMenuWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    if (!IsValid(play_button) || !IsValid(save_games_button) || !IsValid(options_button) ||
        !IsValid(quit_button) || !IsValid(level_select_widget) || !IsValid(save_games_page) ||
        !IsValid(save_game_viewer) || !IsValid(save_games_back_button) ||
        !IsValid(options_widget)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UMainMenuWidget::NativeOnInitialized: One or more bound widgets are "
                    "invalid."));
        return;
    }

    play_button->OnClicked.AddDynamic(this, &ThisClass::handle_play);
    save_games_button->OnClicked.AddDynamic(this, &ThisClass::handle_save_games);
    options_button->OnClicked.AddDynamic(this, &ThisClass::handle_options);
    quit_button->OnClicked.AddDynamic(this, &ThisClass::handle_quit);
    save_games_back_button->OnClicked.AddDynamic(this, &ThisClass::return_from_save_games);
    level_select_widget->back_requested.AddUObject(this, &ThisClass::return_from_level_select);
    options_widget->back_requested.AddUObject(this, &ThisClass::return_from_options);
}

void UMainMenuWidget::NativeConstruct() {
    Super::NativeConstruct();
    auto* const game_instance{GetGameInstance()};
    auto* const subsystem{IsValid(game_instance) ? game_instance->GetSubsystem<UGameSubsystem>()
                                                 : nullptr};
    set_active_page(IsValid(subsystem) && subsystem->has_level_launch_error()
                        ? EMainMenuPage::LevelSelect
                        : EMainMenuPage::Main);
}

void UMainMenuWidget::focus_primary_action() {
    if (!IsValid(play_button)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("UMainMenuWidget::focus_primary_action: Play button is invalid."));
        return;
    }

    play_button->SetKeyboardFocus();
}

void UMainMenuWidget::handle_play() {
    set_active_page(EMainMenuPage::LevelSelect);
}

void UMainMenuWidget::handle_save_games() {
    set_active_page(EMainMenuPage::SaveGames);
    save_game_viewer->focus_primary_action();
}

void UMainMenuWidget::handle_options() {
    set_active_page(EMainMenuPage::Options);
    options_widget->focus_active_tab();
}

void UMainMenuWidget::handle_quit() {
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UMainMenuWidget::return_from_level_select() {
    set_active_page(EMainMenuPage::Main);
    play_button->SetKeyboardFocus();
}

void UMainMenuWidget::return_from_save_games() {
    set_active_page(EMainMenuPage::Main);
    save_games_button->SetKeyboardFocus();
}

void UMainMenuWidget::return_from_options() {
    set_active_page(EMainMenuPage::Main);
    options_button->SetKeyboardFocus();
}

void UMainMenuWidget::set_active_page(EMainMenuPage const page) {
    if (!IsValid(page_switcher) || !IsValid(main_page) || !IsValid(level_select_widget) ||
        !IsValid(save_games_page) || !IsValid(options_widget)) {
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
        case EMainMenuPage::LevelSelect: {
            active_widget = level_select_widget;
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
    if (page == EMainMenuPage::LevelSelect) {
        level_select_widget->activate();
    }
}
}
