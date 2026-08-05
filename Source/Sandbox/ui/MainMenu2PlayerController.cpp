#include "Sandbox/ui/MainMenu2PlayerController.h"

#include "Camera/CameraActor.h"
#include "EngineUtils.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void AMainMenu2PlayerController::BeginPlay() {
    Super::BeginPlay();

    bShowMouseCursor = true;

    for (auto* actor : TActorRange<ACameraActor>(GetWorld())) {
        static FName expected_camera_tag{TEXT("MainMenuCamera")};
        if (actor->ActorHasTag(expected_camera_tag)) {
            camera_actor = actor;
            break;
        }
    }

    if (camera_actor) {
        constexpr float blend_time{0.0f}; // Smooth transition
        SetViewTargetWithBlend(camera_actor, blend_time);
    } else {
        UE_LOG(LogTemp, Warning, TEXT("AMainMenu2PlayerController: No camera actor."));
    }

    spawn_menu_widget();

    // Set input mode to UI only
    FInputModeUIOnly input_mode{};
    if (main_menu_widget_instance) {
        input_mode.SetWidgetToFocus(main_menu_widget_instance->TakeWidget());
    }
    input_mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(input_mode);
}

void AMainMenu2PlayerController::spawn_menu_widget() {
    RETURN_IF_NULLPTR(main_menu_widget_class);

    main_menu_widget_instance = CreateWidget<UMainMenu2Widget>(this, main_menu_widget_class);
    if (main_menu_widget_instance) {
        main_menu_widget_instance->AddToViewport();
    }
}
