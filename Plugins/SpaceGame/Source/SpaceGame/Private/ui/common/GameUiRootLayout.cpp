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
    level_select_focus_id_ = preferred_level_id;
    auto const main_menu_class{ui_data->get_widget_class<UMainMenuWidget>()};
    auto* const main_menu{
        screen_stack->AddWidget<UMainMenuWidget>(main_menu_class, [this](UMainMenuWidget& widget) {
            widget.level_select_requested.RemoveAll(this);
            widget.level_select_requested.AddUObject(this, &ThisClass::show_level_select);
        })};
    if (!IsValid(main_menu)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UGameUiRootLayout::show_main_menu: Failed to push the main menu."));
        return false;
    }

    if (show_level_select_screen) {
        show_level_select();
    } else {
        level_select_focus_id_ = NAME_None;
    }
    return true;
}

auto UGameUiRootLayout::show_pause_menu(UInputAction& toggle_action) -> UPauseMenuWidget* {
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
        [&toggle_action](UPauseMenuWidget& widget) { widget.prepare_for_open(toggle_action); })};
    if (!IsValid(pause_menu)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UGameUiRootLayout::show_pause_menu: Failed to push the pause menu."));
    }
    return pause_menu;
}

auto UGameUiRootLayout::show_level_completion(FString level_display_name)
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
        [name = MoveTemp(level_display_name)](ULevelCompletionWidget& widget) mutable {
            widget.prepare_for_open(MoveTemp(name));
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

void UGameUiRootLayout::show_level_select() {
    auto* const ui_data{ui_data_.Get()};
    if (!IsValid(ui_data) || !IsValid(screen_stack)) {
        return;
    }
    if (IsValid(Cast<ULevelSelectWidget>(screen_stack->GetActiveWidget()))) {
        return;
    }

    auto const level_select_class{ui_data->get_widget_class<ULevelSelectWidget>()};
    auto const preferred_level_id{level_select_focus_id_};
    level_select_focus_id_ = NAME_None;
    if (!IsValid(screen_stack->AddWidget<ULevelSelectWidget>(
            level_select_class, [preferred_level_id](ULevelSelectWidget& widget) {
                widget.prepare_for_open(preferred_level_id);
            }))) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UGameUiRootLayout::show_level_select: Failed to push level select."));
    }
}
}
