#include "Sandbox/player_controllers/MainMenu2PlayerController.h"

void AMainMenu2PlayerController::BeginPlay() {
    Super::BeginPlay();

    bShowMouseCursor = true;

    spawn_menu_widget();
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