#include <SpaceGame/ships/player/MainMenuControlContext.h>

#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>
#include <SpaceGame/ui/main_menu/MainMenuWidget.h>

#include <Camera/CameraActor.h>
#include <EngineUtils.h>

auto FMainMenuControlContext::initialise(ASpaceGamePlayerController& owner,
                                         TSubclassOf<ml::ioj::UMainMenuWidget> const widget_class)
    -> bool {
    if (initialised_) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FMainMenuControlContext::initialise: Context is already initialised."));
        return false;
    }
    if (!widget_class) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FMainMenuControlContext::initialise: Widget class is not configured."));
        return false;
    }

    auto* const widget{
        CreateWidget<ml::ioj::UMainMenuWidget>(&owner, widget_class, TEXT("main_menu"))};
    if (!IsValid(widget)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FMainMenuControlContext::initialise: Failed to create the main menu."));
        return false;
    }

    owner_ = &owner;
    widget_ = widget;
    initialised_ = true;
    return true;
}

auto FMainMenuControlContext::can_bind() const -> bool {
    return initialised_ && owner_.IsValid() && IsValid(widget_);
}

auto FMainMenuControlContext::bind() -> bool {
    if (bound_) {
        return true;
    }
    if (!can_bind()) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FMainMenuControlContext::bind: Context dependencies are invalid."));
        return false;
    }

    auto* const owner{owner_.Get()};
    select_camera();
    widget_->AddToViewport();

    FInputModeUIOnly input_mode;
    input_mode.SetWidgetToFocus(widget_->TakeWidget());
    input_mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    owner->SetInputMode(input_mode);
    owner->SetShowMouseCursor(true);
    widget_->focus_primary_action();
    bound_ = true;
    return true;
}

void FMainMenuControlContext::unbind() {
    if (!bound_) {
        return;
    }

    bound_ = false;
    if (IsValid(widget_)) {
        widget_->RemoveFromParent();
    }
}

void FMainMenuControlContext::shutdown() {
    unbind();
    widget_ = nullptr;
    owner_.Reset();
    initialised_ = false;
}

void FMainMenuControlContext::select_camera() {
    auto* const owner{owner_.Get()};
    if (!IsValid(owner)) {
        return;
    }

    static FName const camera_tag{TEXT("MainMenuCamera")};
    for (TActorIterator<ACameraActor> it{owner->GetWorld()}; it; ++it) {
        if (it->ActorHasTag(camera_tag)) {
            owner->SetViewTarget(*it);
            return;
        }
    }

    UE_LOG(LogSandboxController,
           Warning,
           TEXT("FMainMenuControlContext::select_camera: No camera tagged '%s' was found."),
           *camera_tag.ToString());
}
