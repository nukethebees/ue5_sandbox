#include "SpaceGame/ui/common/GameUiRootLayout.h"

#include "SpaceGame/presentation/TestBatchGameUiData.h"
#include "SpaceGame/support/logging/SandboxLogCategories.h"
#include "SpaceGame/ui/LevelCompletionWidget.h"
#include "SpaceGame/ui/main_menu/LevelSelectWidget.h"
#include "SpaceGame/ui/main_menu/MainMenuWidget.h"
#include "SpaceGame/ui/PauseMenuWidget.h"

#include <Input/UIActionBindingHandle.h>
#include <InputAction.h>
#include <Widgets/CommonActivatableWidgetContainer.h>

namespace ml::ioj {
auto UGameUiRootLayout::initialise(UTestBatchGameUiData& ui_data) -> bool {
    if (!IsValid(screen_stack) || !IsValid(modal_stack)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UGameUiRootLayout::initialise: Activatable stacks are invalid."));
        return false;
    }
    ui_data_ = &ui_data;
    return true;
}

auto UGameUiRootLayout::show_main_menu(bool const show_level_select_screen,
                                       FName const preferred_level_id) -> bool {
    auto* const ui_data{ui_data_.Get()};
    if (!IsValid(ui_data) || !IsValid(screen_stack)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UGameUiRootLayout::show_main_menu: Root is not initialised."));
        return false;
    }

    screen_stack->ClearWidgets();
    auto const main_menu_class{ui_data->get_widget_class<UMainMenuWidget>()};
    auto const level_select_class{ui_data->get_widget_class<ULevelSelectWidget>()};
    auto* const main_menu{screen_stack->AddWidget<UMainMenuWidget>(
        main_menu_class,
        [level_select_class, show_level_select_screen, preferred_level_id](
            UMainMenuWidget& widget) {
            widget.prepare_for_open(
                level_select_class, show_level_select_screen, preferred_level_id);
        })};
    if (!IsValid(main_menu)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UGameUiRootLayout::show_main_menu: Failed to push the main menu."));
        return false;
    }

    return true;
}

auto UGameUiRootLayout::show_pause_menu(UInputAction& toggle_action, FPauseMenuData data)
    -> UPauseMenuWidget* {
    auto* const ui_data{ui_data_.Get()};
    if (!IsValid(ui_data) || !IsValid(modal_stack)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UGameUiRootLayout::show_pause_menu: Root is not initialised."));
        return nullptr;
    }
    if (auto* const active_pause{Cast<UPauseMenuWidget>(modal_stack->GetActiveWidget())};
        IsValid(active_pause)) {
        return active_pause;
    }
    if (IsValid(modal_stack->GetActiveWidget())) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("UGameUiRootLayout::show_pause_menu: Another modal is already active."));
        return nullptr;
    }

    auto const pause_menu_class{ui_data->get_widget_class<UPauseMenuWidget>()};
    auto* const pause_menu{modal_stack->AddWidget<UPauseMenuWidget>(
        pause_menu_class,
        [&toggle_action, data = MoveTemp(data)](UPauseMenuWidget& widget) mutable {
            widget.prepare_for_open(toggle_action, MoveTemp(data));
        })};
    if (!IsValid(pause_menu)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UGameUiRootLayout::show_pause_menu: Failed to push the pause menu."));
    }
    return pause_menu;
}

auto UGameUiRootLayout::show_level_completion(FString level_display_name,
                                              FLevelTelemetrySnapshot snapshot)
    -> ULevelCompletionWidget* {
    auto* const ui_data{ui_data_.Get()};
    if (!IsValid(ui_data) || !IsValid(modal_stack)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UGameUiRootLayout::show_level_completion: Root is not initialised."));
        return nullptr;
    }
    if (auto* const active_completion{Cast<ULevelCompletionWidget>(modal_stack->GetActiveWidget())};
        IsValid(active_completion)) {
        return active_completion;
    }
    if (IsValid(modal_stack->GetActiveWidget())) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("UGameUiRootLayout::show_level_completion: Another modal is already active."));
        return nullptr;
    }

    auto const completion_class{ui_data->get_widget_class<ULevelCompletionWidget>()};
    auto* const completion{modal_stack->AddWidget<ULevelCompletionWidget>(
        completion_class,
        [name = MoveTemp(level_display_name),
         snapshot = MoveTemp(snapshot)](ULevelCompletionWidget& widget) mutable {
            widget.prepare_for_open(MoveTemp(name), MoveTemp(snapshot));
        })};
    if (!IsValid(completion)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UGameUiRootLayout::show_level_completion: Failed to push completion."));
    }
    return completion;
}

void UGameUiRootLayout::clear_menus() {
    if (IsValid(modal_stack)) {
        modal_stack->ClearWidgets();
    }
    if (IsValid(screen_stack)) {
        screen_stack->ClearWidgets();
    }
}

auto UGameUiRootLayout::get_active_screen() const -> UCommonActivatableWidget* {
    return IsValid(screen_stack) ? screen_stack->GetActiveWidget() : nullptr;
}

auto UGameUiRootLayout::get_active_modal() const -> UCommonActivatableWidget* {
    return IsValid(modal_stack) ? modal_stack->GetActiveWidget() : nullptr;
}

auto UGameUiRootLayout::get_screen_count() const -> int32 {
    return IsValid(screen_stack) ? screen_stack->GetNumWidgets() : 0;
}

auto UGameUiRootLayout::get_modal_count() const -> int32 {
    return IsValid(modal_stack) ? modal_stack->GetNumWidgets() : 0;
}

TOptional<FUIInputConfig> UGameUiRootLayout::GetDesiredInputConfig() const {
    if (IsValid(screen_stack) && screen_stack->GetNumWidgets() > 0) {
        auto config{FUIInputConfig{ECommonInputMode::Menu,
                                   EMouseCaptureMode::NoCapture,
                                   EMouseLockMode::DoNotLock,
                                   false}};
        config.bIgnoreMoveInput = true;
        config.bIgnoreLookInput = true;
        return config;
    }

    return FUIInputConfig{ECommonInputMode::Game,
                          EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown,
                          EMouseLockMode::LockOnCapture,
                          true};
}

}
