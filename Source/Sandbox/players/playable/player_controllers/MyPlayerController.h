// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "Sandbox/input/mixins/EnhancedInputMixin.hpp"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/playable/characters/MyCharacter.h"
#include "Sandbox/players/playable/data/MyPlayerControllerCache.h"
#include "Sandbox/players/playable/data/MyPlayerControllerInputActions.h"

#include "MyPlayerController.generated.h"

struct FInputActionValue;
class UInputMappingContext;
class UWorld;

struct FActorCorners;

class UInventoryComponent;

class UMainHUDWidget;
class SInGameMenuWidget;
class UInGamePlayerMenu;

struct FActorCorners;

USTRUCT(BlueprintType)
struct FMyPlayerControllerHud {
    GENERATED_BODY()

    // Widget classes
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UMainHUDWidget> main_widget_class;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UInGamePlayerMenu> umg_player_menu_class;

    // Widget instances
    UPROPERTY(VisibleAnywhere, Category = "UI")
    UMainHUDWidget* main_widget;

    UPROPERTY(VisibleAnywhere, Category = "UI")
    UInGamePlayerMenu* umg_player_menu{nullptr};

    // Slate widgets
    TSharedPtr<SInGameMenuWidget> in_game_menu_widget;
    bool is_in_game_menu_open{false};

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    bool use_umg_player_menu{true};
};

UCLASS()
class SANDBOX_API AMyPlayerController
    : public APlayerController
    , public ml::LogMsgMixin<"MyPlayerController", LogSandboxActor>
    , public ml::EnhancedInputMixin {
    GENERATED_BODY()
  public:
    AMyPlayerController() = default;
  protected:
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void Tick(float DeltaSeconds) override;
  public:
    virtual void SetupInputComponent() override;

    // Movement
    UFUNCTION()
    void move(FInputActionValue const& value);
    UFUNCTION()
    void start_jump();
    UFUNCTION()
    void stop_jump();
    UFUNCTION()
    void start_crouch();
    UFUNCTION()
    void stop_crouch();
    UFUNCTION()
    void start_sprint();
    UFUNCTION()
    void stop_sprint();
    UFUNCTION()
    void warp_to_cursor(FInputActionValue const& value);
    UFUNCTION()
    void start_jetpack();
    UFUNCTION()
    void stop_jetpack();

    // Vision
    UFUNCTION()
    void look(FInputActionValue const& value);
    UFUNCTION()
    void cycle_camera();

    // Combat
    UFUNCTION()
    void attack_started();
    UFUNCTION()
    void attack_continued(FInputActionInstance const& instance);
    UFUNCTION()
    void attack_ended();

    // Torch
    UFUNCTION()
    void toggle_torch();
    UFUNCTION()
    void scroll_torch_cone(FInputActionValue const& value);

    // Inventory
    UFUNCTION()
    void cycle_next_weapon();
    UFUNCTION()
    void cycle_prev_weapon();
    UFUNCTION()
    void unequip_weapon();
    UFUNCTION()
    void drop_weapon();
    UFUNCTION()
    void reload_weapon();

    // Interaction
    UFUNCTION()
    void toggle_mouse();
    UFUNCTION()
    void mouse_click(FInputActionValue const& value);
    UFUNCTION()
    void interact();
    UFUNCTION()
    void drop_waypoint();

    // UI
    void initialise_hud(UWorld& world);
    UFUNCTION()
    void toggle_in_game_menu();
    UFUNCTION()
    void set_game_input_mode();
    UFUNCTION()
    void set_mouse_input_mode();
    UFUNCTION()
    void toggle_hud();

    // UI/HUD
    UFUNCTION()
    void update_jump(int32 new_jump);
    UFUNCTION()
    void update_fuel(FJetpackState const& jetpack_state);
    UFUNCTION()
    void update_coin(int32 data);
    UFUNCTION()
    void update_health(FHealthData health_data);
    UFUNCTION()
    void update_max_speed(float data);
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
    UFUNCTION()
    void update_target_screen_bounds(FActorCorners const& corners);
    UFUNCTION()
    void clear_target_screen_bounds();

    UPROPERTY()
    AMyCharacter* controlled_character;
    UPROPERTY()
    FMyPlayerControllerCache cache;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    FMyPlayerControllerInputActions input{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Description")
    float description_scan_interval{0.15f};

    UPROPERTY(EditAnywhere, Category = "UI")
    FMyPlayerControllerHud hud;
  private:
    // Input
    void add_input_mapping_context(UInputMappingContext* context);
    void swap_input_mapping_context(UInputMappingContext* to_remove, UInputMappingContext* to_add);

    // UI
    void perform_description_scan();

    bool tick_no_controller_character_warning_fired{false};
    float attack_elapsed_time{0.0f};
    FTimerHandle description_scanner_timer_handle;
    bool tracking_target_outline{false};
};
