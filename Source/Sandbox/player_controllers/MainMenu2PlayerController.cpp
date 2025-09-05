#include "Sandbox/player_controllers/MainMenu2PlayerController.h"

void AMainMenu2PlayerController::BeginPlay() {
    Super::BeginPlay();

    bShowMouseCursor = true;

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
    if (!main_menu_widget_class) {
        UE_LOG(LogTemp, Warning, TEXT("Menu widget class not set on MainMenuController"));
        return;
    }

    main_menu_widget_instance = CreateWidget<UMainMenu2Widget>(this, main_menu_widget_class);
    if (main_menu_widget_instance) {
        main_menu_widget_instance->AddToViewport();
    }
}
