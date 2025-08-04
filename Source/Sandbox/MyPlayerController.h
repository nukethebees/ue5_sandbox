// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "MyCharacter.h"
#include "print_msg_mixin.hpp"
#include "Engine/GameViewportClient.h"

#include "MyPlayerController.generated.h"

/**
 *
 */
UCLASS()
class SANDBOX_API AMyPlayerController
    : public APlayerController
    , public print_msg_mixin {
    GENERATED_BODY()
  public:
    AMyPlayerController() = default;
  protected:
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void Tick(float DeltaSeconds) override;
  public:
    virtual void SetupInputComponent() override;

    UPROPERTY()
    AMyCharacter* controlled_character;

    UPROPERTY(EditAnywhere, Category = "Input")
    TSoftObjectPtr<UInputMappingContext> default_mapping_context;
    UPROPERTY(EditAnywhere, Category = "Input")
    TSoftObjectPtr<UInputAction> look_action;
    UPROPERTY(EditAnywhere, Category = "Input")
    TSoftObjectPtr<UInputAction> toggle_mouse_action;
    UPROPERTY(EditAnywhere, Category = "Input")
    TSoftObjectPtr<UInputAction> mouse_click_action;
    UPROPERTY(EditAnywhere, Category = "Input")
    TSoftObjectPtr<UInputAction> toggle_torch_action;

    UFUNCTION()
    void look(FInputActionValue const& value);
    UFUNCTION()
    void toggle_mouse(FInputActionValue const& value);
    UFUNCTION()
    void mouse_click(FInputActionValue const& value);
    UFUNCTION()
    void toggle_torch(FInputActionValue const& value);
};
