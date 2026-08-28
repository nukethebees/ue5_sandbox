#include "SpaceGame/ui/main_menu/MainMenuPlayerController.h"

#include "SpaceGame/support/logging/SandboxLogCategories.h"
#include "SpaceGame/ui/main_menu/MainMenuWidget.h"

#include <Camera/CameraActor.h>
#include <EngineUtils.h>
#include <UObject/ConstructorHelpers.h>

namespace ml::ioj {
AMainMenuPlayerController::AMainMenuPlayerController() {
    static ConstructorHelpers::FClassFinder<UMainMenuWidget> const widget_class{
        TEXT("/SpaceGame/UI/MainMenu/WBP_MainMenu")};
    main_menu_widget_class = widget_class.Class;
}

void AMainMenuPlayerController::BeginPlay() {
    Super::BeginPlay();

    select_menu_camera();
    create_main_menu();
}

void AMainMenuPlayerController::select_menu_camera() {
    static FName const menu_camera_tag{TEXT("MainMenuCamera")};
    for (auto* const camera : TActorRange<ACameraActor>(GetWorld())) {
        if (IsValid(camera) && camera->ActorHasTag(menu_camera_tag)) {
            SetViewTarget(camera);
            return;
        }
    }

    UE_LOG(LogSandboxUI,
           Warning,
           TEXT("AMainMenuPlayerController::select_menu_camera: No camera tagged '%s' was found."),
           *menu_camera_tag.ToString());
}

void AMainMenuPlayerController::create_main_menu() {
    if (!main_menu_widget_class) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("AMainMenuPlayerController::create_main_menu: Main menu widget class is "
                    "invalid."));
        return;
    }

    auto* const widget{
        CreateWidget<UMainMenuWidget>(this, main_menu_widget_class, TEXT("main_menu"))};
    if (!IsValid(widget)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("AMainMenuPlayerController::create_main_menu: Failed to create the main "
                    "menu widget."));
        return;
    }

    main_menu_widget_ = widget;
    main_menu_widget_->AddToViewport();

    bShowMouseCursor = true;
    FInputModeUIOnly input_mode{};
    input_mode.SetWidgetToFocus(main_menu_widget_->TakeWidget());
    input_mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(input_mode);
    main_menu_widget_->focus_primary_action();
}
}
