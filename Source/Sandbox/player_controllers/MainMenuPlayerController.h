// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Sandbox/widgets/MainMenuWidget.h"

#include "MainMenuPlayerController.generated.h"

UCLASS()
class SANDBOX_API AMainMenuPlayerController : public APlayerController {
    GENERATED_BODY()
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UMainMenuWidget> main_menu_widget_class;
  private:
    UPROPERTY()
    UMainMenuWidget* main_menu_widget_instance;
};
