// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <utility>

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"

#include "MyCharacter.generated.h"

class UInputMappingContext;
class UInputAction;
class UInputComponent;

UCLASS()
class SANDBOX_API AMyCharacter : public ACharacter {
    GENERATED_BODY()
  public:
    AMyCharacter();
private:
    void print_msg(FString const & msg);
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputMappingContext> first_person_context;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputAction> move_action;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputAction> jump_action;
  public:
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    UFUNCTION()
    void move(FInputActionValue const& value);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float move_speed{100.0f};
};
