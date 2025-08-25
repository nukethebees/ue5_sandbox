// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Sandbox/actor_components/HealthComponent.h"
#include "Sandbox/widgets/MainHUDWidget.h"

#include "MyHUD.generated.h"

UCLASS()
class SANDBOX_API AMyHUD : public AHUD {
    GENERATED_BODY()
  public:
    virtual void BeginPlay() override;
  protected:
    // Widget classes
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UMainHUDWidget> main_widget_class;

    // Widget instances
    UPROPERTY()
    UMainHUDWidget* main_widget;
  public:
    UFUNCTION()
    void update_fuel(float new_fuel);
    UFUNCTION()
    void update_jump(int32 new_jump);
    UFUNCTION()
    void update_coin(int32 new_coin_count);
    UFUNCTION()
    void update_health(FHealthData health_data);
};
