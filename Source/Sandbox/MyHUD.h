// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Sandbox/widgets/FuelWidget.h"
#include "Sandbox/widgets/JumpWidget.h"

#include "MyHUD.generated.h"

UCLASS()
class SANDBOX_API AMyHUD : public AHUD {
    GENERATED_BODY()
  public:
    virtual void BeginPlay() override;
  protected:
    // Widget classes
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UFuelWidget> fuel_widget_class;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UJumpWidget> jump_widget_class;

    // Widget instances
    UPROPERTY()
    UFuelWidget* fuel_widget;

    UPROPERTY()
    UJumpWidget* jump_widget;
  public:
    // Update methods
    void update_fuel(float new_fuel);
    void update_jump(int32 new_jump);
};
