#include "SpaceGame/ui/common/GameUiRootLayout.h"

#include "SpaceGame/presentation/TestBatchGameUiData.h"
#include "SpaceGame/ui/LevelCompletionWidget.h"
#include "SpaceGame/ui/main_menu/LevelSelectWidget.h"
#include "SpaceGame/ui/main_menu/MainMenuWidget.h"
#include "SpaceGame/ui/PauseMenuWidget.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>

#include <Components/Overlay.h>
#include <Components/OverlaySlot.h>

namespace ioj {
namespace {
void show_widget(UOverlay& panel, UMenuActivatableWidget& widget) {
    auto* const slot{panel.AddChildToOverlay(&widget)};
    slot->SetHorizontalAlignment(HAlign_Fill);
    slot->SetVerticalAlignment(VAlign_Fill);
    if (auto* target{widget.focus_target()}) {
        target->SetFocus();
    }
}
}
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

    screen_stack->ClearChildren();
    auto const main_menu_class{ui_data->get_widget_class<UMainMenuWidget>()};
    auto const level_select_class{ui_data->get_widget_class<ULevelSelectWidget>()};
    auto* const main_menu{CreateWidget<UMainMenuWidget>(GetOwningPlayer(), main_menu_class)};
    if (!IsValid(main_menu)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UGameUiRootLayout::show_main_menu: Failed to push the main menu."));
        return false;
    }

    main_menu->prepare_for_open(level_select_class, show_level_select_screen, preferred_level_id);
    show_widget(*screen_stack, *main_menu);
    return true;
}

auto UGameUiRootLayout::show_pause_menu() -> UPauseMenuWidget* {
    auto* const ui_data{ui_data_.Get()};
    if (!IsValid(ui_data) || !IsValid(modal_stack)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UGameUiRootLayout::show_pause_menu: Root is not initialised."));
        return nullptr;
    }
    if (auto* const active_pause{Cast<UPauseMenuWidget>(modal_stack->GetChildAt(0))};
        IsValid(active_pause)) {
        return active_pause;
    }
    if (IsValid(modal_stack->GetChildAt(0))) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("UGameUiRootLayout::show_pause_menu: Another modal is already active."));
        return nullptr;
    }

    auto const pause_menu_class{ui_data->get_widget_class<UPauseMenuWidget>()};
    auto* const pause_menu{CreateWidget<UPauseMenuWidget>(GetOwningPlayer(), pause_menu_class)};
    if (!IsValid(pause_menu)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UGameUiRootLayout::show_pause_menu: Failed to push the pause menu."));
    }
    if (IsValid(pause_menu)) {
        pause_menu->prepare_for_open();
        show_widget(*modal_stack, *pause_menu);
    }
    return pause_menu;
}

auto UGameUiRootLayout::show_level_completion(FLevelCompletion data) -> ULevelCompletionWidget* {
    auto* const ui_data{ui_data_.Get()};
    if (!IsValid(ui_data) || !IsValid(modal_stack)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UGameUiRootLayout::show_level_completion: Root is not initialised."));
        return nullptr;
    }
    if (auto* const active_completion{Cast<ULevelCompletionWidget>(modal_stack->GetChildAt(0))};
        IsValid(active_completion)) {
        return active_completion;
    }
    if (IsValid(modal_stack->GetChildAt(0))) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("UGameUiRootLayout::show_level_completion: Another modal is already active."));
        return nullptr;
    }

    auto const completion_class{ui_data->get_widget_class<ULevelCompletionWidget>()};
    auto* const completion{
        CreateWidget<ULevelCompletionWidget>(GetOwningPlayer(), completion_class)};
    if (!IsValid(completion)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UGameUiRootLayout::show_level_completion: Failed to push completion."));
    }
    if (IsValid(completion)) {
        completion->prepare_for_open(MoveTemp(data));
        show_widget(*modal_stack, *completion);
    }
    return completion;
}

auto UGameUiRootLayout::main_menu() const -> UMainMenuWidget* {
    return Cast<UMainMenuWidget>(screen_stack->GetChildAt(0));
}

void UGameUiRootLayout::clear_menus() {
    if (IsValid(modal_stack)) {
        modal_stack->ClearChildren();
    }
    if (IsValid(screen_stack)) {
        screen_stack->ClearChildren();
    }
}

}
