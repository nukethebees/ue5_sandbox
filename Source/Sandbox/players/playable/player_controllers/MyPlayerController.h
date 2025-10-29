// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"

#include "Sandbox/input/mixins/EnhancedInputMixin.hpp"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/playable/characters/MyCharacter.h"
#include "Sandbox/players/playable/data/MyPlayerControllerCache.h"
#include "Sandbox/players/playable/data/MyPlayerControllerInputActions.h"

#include "MyPlayerController.generated.h"

class AMyHUD;

struct FActorCorners;

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
    UFUNCTION()
    void toggle_in_game_menu();
    UFUNCTION()
    void set_game_input_mode();
    UFUNCTION()
    void set_mouse_input_mode();
    UFUNCTION()
    void update_target_screen_bounds(FActorCorners const& corners);
    UFUNCTION()
    void clear_target_screen_bounds();

    UPROPERTY()
    AMyCharacter* controlled_character;
    UPROPERTY()
    AMyHUD* hud;
    UPROPERTY()
    FMyPlayerControllerCache cache;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    FMyPlayerControllerInputActions input{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Description")
    float description_scan_interval{0.15f};
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
