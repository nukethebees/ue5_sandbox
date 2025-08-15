// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/player_controllers/MainMenuPlayerController.h"

void AMainMenuPlayerController::BeginPlay() {
    Super::BeginPlay();

    bShowMouseCursor = true;

    if (main_menu_widget_class) {
        main_menu_widget_instance = CreateWidget<UMainMenuWidget>(this, main_menu_widget_class);
        main_menu_widget_instance->AddToViewport();
    }
}
