#include <SpaceGame/ships/player/MenuControlContext.h>

#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/ships/player/TestSpaceShipController.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>
#include <SpaceGame/ui/PauseMenuWidget.h>

auto FMenuControlContext::initialise(ATestSpaceShipController& owner, UTestBatchGameUiData& ui_data)
    -> bool {
    if (initialised_) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FMenuControlContext::initialise: Context is already initialised."));
        return false;
    }

    auto const widget_class{ui_data.get_widget_class<ml::ioj::UPauseMenuWidget>()};
    if (!widget_class) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FMenuControlContext::initialise: Widget class is not configured."));
        return false;
    }

    auto* const created_widget{
        CreateWidget<ml::ioj::UPauseMenuWidget>(&owner, widget_class, TEXT("pause_menu"))};
    if (!IsValid(created_widget)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FMenuControlContext::initialise: Failed to create widget."));
        return false;
    }

    owner_ = &owner;
    pause_menu_widget = created_widget;
    created_widget->resume_requested.AddUObject(&owner, &ATestSpaceShipController::resume_game);
    initialised_ = true;
    return true;
}

auto FMenuControlContext::can_bind() const -> bool {
    return initialised_ && owner_.IsValid() && IsValid(pause_menu_widget);
}

auto FMenuControlContext::bind() -> bool {
    if (bound_) {
        return true;
    }
    if (!can_bind()) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FMenuControlContext::bind: Context dependencies are invalid."));
        return false;
    }

    auto* const owner{owner_.Get()};
    pause_menu_widget->AddToViewport(100);

    FInputModeGameAndUI input_mode;
    input_mode.SetWidgetToFocus(pause_menu_widget->TakeWidget());
    input_mode.SetHideCursorDuringCapture(false);
    owner->SetInputMode(input_mode);
    owner->SetShowMouseCursor(true);
    pause_menu_widget->focus_resume_button();
    bound_ = true;
    return true;
}

void FMenuControlContext::unbind() {
    if (!bound_) {
        return;
    }

    bound_ = false;
    if (IsValid(pause_menu_widget)) {
        pause_menu_widget->RemoveFromParent();
    }
    if (auto* const owner{owner_.Get()}) {
        owner->SetInputMode(FInputModeGameOnly{});
        owner->SetShowMouseCursor(false);
    }
}

void FMenuControlContext::shutdown() {
    unbind();
    if (IsValid(pause_menu_widget)) {
        if (auto* const owner{owner_.Get()}) {
            pause_menu_widget->resume_requested.RemoveAll(owner);
        }
        pause_menu_widget->RemoveFromParent();
    }
    pause_menu_widget = nullptr;
    owner_.Reset();
    initialised_ = false;
}
