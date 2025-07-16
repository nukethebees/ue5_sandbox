// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "MyCharacter.h"

#include "MyPlayerController.generated.h"

/**
 *
 */
UCLASS()
class SANDBOX_API AMyPlayerController : public APlayerController {
    GENERATED_BODY()
  public:
    AMyPlayerController() = default;
  private:
    void print_msg(FString const& msg);
  protected:
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
  public:
    virtual void SetupInputComponent() override;

    UPROPERTY()
    AMyCharacter* controlled_character;

    UPROPERTY(EditAnywhere, Category = "Input")
    TSoftObjectPtr<UInputMappingContext> default_mapping_context;
    UPROPERTY(EditAnywhere, Category = "Input")
    TSoftObjectPtr<UInputAction> look_action;

    UFUNCTION()
    void look(FInputActionValue const& value);
};
