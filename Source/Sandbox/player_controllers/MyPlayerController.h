// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Sandbox/characters/MyCharacter.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/mixins/print_msg_mixin.hpp"
#include "Sandbox/mixins/EnhancedInputMixin.hpp"

#include "MyPlayerController.generated.h"

USTRUCT(BlueprintType)
struct FMyPlayerControllerInputActions {
    GENERATED_BODY()

    FMyPlayerControllerInputActions() = default;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* base_context{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* direct_mode_context{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* cursor_mode_context{nullptr};

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* look{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* attack{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* toggle_mouse{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* mouse_click{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* warp_to_cursor{nullptr};
};

UCLASS()
class SANDBOX_API AMyPlayerController
    : public APlayerController
    , public print_msg_mixin
    , public ml::LogMsgMixin<"MyPlayerController">
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

    UFUNCTION()
    void look(FInputActionValue const& value);
    UFUNCTION()
    void attack();
    UFUNCTION()
    void toggle_mouse();
    UFUNCTION()
    void mouse_click(FInputActionValue const& value);
    UFUNCTION()
    void warp_to_cursor(FInputActionValue const& value);

    UPROPERTY()
    AMyCharacter* controlled_character;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    FMyPlayerControllerInputActions input{};
  private:
    void set_game_input_mode();
    void set_mouse_input_mode();

    bool tick_no_controller_character_warning_fired{false};
    void add_input_mapping_context(UInputMappingContext* context);
    void swap_input_mapping_context(UInputMappingContext* to_remove, UInputMappingContext* to_add);
};
