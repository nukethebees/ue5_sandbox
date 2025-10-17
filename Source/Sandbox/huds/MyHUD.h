// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "Sandbox/data/health/HealthData.h"
#include "Sandbox/data/JetpackState.h"
#include "Sandbox/mixins/LogMsgMixin.hpp"
#include "Sandbox/SandboxLogCategories.h"
#include "Sandbox/widgets/MainHUDWidget.h"

#include "MyHUD.generated.h"

class SInGameMenuWidget;

UCLASS()
class SANDBOX_API AMyHUD
    : public AHUD
    , public ml::LogMsgMixin<"AMyHUD", LogSandboxUI> {
    GENERATED_BODY()
  public:
    AMyHUD();

    virtual void BeginPlay() override;
  protected:
    // Widget classes
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UMainHUDWidget> main_widget_class;

    // Widget instances
    UPROPERTY()
    UMainHUDWidget* main_widget;

    // Slate widgets
    TSharedPtr<SInGameMenuWidget> in_game_menu_widget;
    bool is_in_game_menu_open{false};
  public:
    UFUNCTION()
    void toggle_in_game_menu();
    UFUNCTION()
    void update_fuel(FJetpackState const& jetpack_state);
    UFUNCTION()
    void update_jump(int32 new_jump);
    UFUNCTION()
    void update_coin(int32 data) {
        if (main_widget && main_widget->coin_widget) {
            main_widget->coin_widget->update(data);
        } else {
            UE_LOGFMT(LogTemp, Warning, "No coin widget.");
        }
    }
    UFUNCTION()
    void update_health(FHealthData health_data);
    UFUNCTION()
    void update_max_speed(float data) {
        if (main_widget && main_widget->max_speed_widget) {
            main_widget->max_speed_widget->update(data);
        }
    }
    UFUNCTION()
    void update_ammo(int32 ammo_count);
};
