// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Sandbox/data/SpeedBoost.h"

#include "SpeedBoostItemComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API USpeedBoostItemComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    USpeedBoostItemComponent();

    UFUNCTION(BlueprintCallable)
    void trigger_effect(AActor* target_actor);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    FSpeedBoost speed_boost{};
};
