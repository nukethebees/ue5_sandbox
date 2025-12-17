// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Logging/StructuredLog.h"

#include "Sandbox/combat/weapons/data/AmmoData.h"
#include "Sandbox/environment/data/ActorCorners.h"
#include "Sandbox/health/data/HealthData.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/playable/data/JetpackState.h"
#include "Sandbox/ui/hud/widgets/umg/MainHUDWidget.h"
#include "Sandbox/ui/widgets/slate/NumWidget.h"
#include "Sandbox/ui/widgets/umg/ValueWidget.h"

#include "MyHUD.generated.h"

class UInventoryComponent;

class SInGameMenuWidget;
class UInGamePlayerMenu;

struct FActorCorners;

class AMyCharacter;

UCLASS()
class SANDBOX_API AMyHUD
    : public AHUD
    , public ml::LogMsgMixin<"AMyHUD", LogSandboxUI> {
    GENERATED_BODY()
  public:
    AMyHUD();

    virtual void BeginPlay() override;

    void set_inventory(UInventoryComponent& inventory);
    void set_character(AMyCharacter& my_char);

    UFUNCTION()
    void toggle_in_game_menu();
    UFUNCTION()
    void update_max_speed(float data) {
        if (main_widget && main_widget->max_speed_widget) {
            main_widget->max_speed_widget->update(data);
        }
    }
    UFUNCTION()
    void update_jump(int32 new_jump);
    UFUNCTION()
    void update_target_screen_bounds(FActorCorners const& corners);
    UFUNCTION()
    void clear_target_screen_bounds();
  protected:
    UFUNCTION()
    void update_fuel(FJetpackState const& jetpack_state);
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
    void update_current_ammo(FAmmoData current_ammo);
    UFUNCTION()
    void update_reserve_ammo(FAmmoData ammo);
    UFUNCTION()
    void on_weapon_reloaded(FAmmoData weapon_ammo, FAmmoData reserve_ammo);
    UFUNCTION()
    void on_weapon_equipped(FAmmoData weapon_ammo, FAmmoData max_ammo, FAmmoData reserve_ammo);
    UFUNCTION()
    void on_weapon_unequipped();

    UFUNCTION()
    void update_description(FText const& text);

    // Widget classes
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UMainHUDWidget> main_widget_class;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UInGamePlayerMenu> umg_player_menu_class;

    // Widget instances
    UPROPERTY()
    UMainHUDWidget* main_widget;

    UPROPERTY()
    UInGamePlayerMenu* umg_player_menu{nullptr};

    // Slate widgets
    TSharedPtr<SInGameMenuWidget> in_game_menu_widget;
    bool is_in_game_menu_open{false};

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    bool use_umg_player_menu{true};
};
