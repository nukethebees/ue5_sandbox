// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <utility>

#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/SpotLightComponent.h"
#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Sandbox/widgets/FuelWidget.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputActionValue.h"
#include "Sandbox/widgets/JumpWidget.h"
#include "WarpComponent.h"

#include "MyCharacter.generated.h"

class UInputMappingContext;
class UInputAction;
class UInputComponent;
class AMyPlayerController;

UCLASS()
class SANDBOX_API AMyCharacter : public ACharacter {
    GENERATED_BODY()
  public:
    AMyCharacter();
  private:
    void print_msg(FString const& msg);
  protected:
    virtual void BeginPlay() override;

    // Input
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputMappingContext> first_person_context;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputAction> move_action;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputAction> jump_action;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputAction> look_action;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputAction> jetpack_action;

    // UI
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UFuelWidget> hud_fuel_widget_class;
    UPROPERTY()
    UFuelWidget* hud_fuel_widget{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UJumpWidget> hud_jump_widget_class;
    UPROPERTY()
    UJumpWidget* hud_jump_widget{nullptr};
  public:
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    UPROPERTY()
    AMyPlayerController* my_player_controller;

    UFUNCTION()
    void move(FInputActionValue const& value);
    UFUNCTION()
    void look(FInputActionValue const& value);
    UFUNCTION()
    void start_jetpack(FInputActionValue const& value);
    UFUNCTION()
    void stop_jetpack(FInputActionValue const& value);

    UPROPERTY(VisibleAnywhere, Category = Camera)
    UCameraComponent* first_person_camera_component;

    // Movement
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float move_speed{800.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float acceleration{100.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float jetpack_fuel{0.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float jetpack_fuel_max{2.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float jetpack_fuel_recharge_rate{1.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float jetpack_fuel_consumption_rate{1.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float jetpack_force{600.0f};

    // Torch
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Torch")
    USpotLightComponent* torch_component;
    UFUNCTION()
    void aim_torch(FVector const& world_location);
    UFUNCTION()
    void reset_torch();
    UFUNCTION()
    void toggle_torch();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
    UWarpComponent* warp_component{nullptr};
  private:
    float jetpack_fuel_previous{0.0f};
    bool is_jetpacking{false};
    bool torch_on{true};
};
