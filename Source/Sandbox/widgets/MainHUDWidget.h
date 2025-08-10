// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "CoreMinimal.h"
#include "Sandbox/widgets/FuelWidget.h"
#include "Sandbox/widgets/JumpWidget.h"

#include "MainHUDWidget.generated.h"

UCLASS()
class SANDBOX_API UMainHUDWidget : public UUserWidget {
    GENERATED_BODY()
  public:
  protected:
    // Widget instances
    UPROPERTY(meta = (BindWidget))
    UFuelWidget* fuel_widget;

    UPROPERTY(meta = (BindWidget))
    UJumpWidget* jump_widget;
  public:
    UFUNCTION(BlueprintCallable)
    void update_fuel(float new_fuel);
    UFUNCTION(BlueprintCallable)
    void update_jump(int32 new_jump);
};
