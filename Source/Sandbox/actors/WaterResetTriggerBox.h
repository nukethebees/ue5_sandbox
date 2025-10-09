// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "GameFramework/PlayerStart.h"

#include "WaterResetTriggerBox.generated.h"

UCLASS()
class SANDBOX_API AWaterResetTriggerBox : public ATriggerBox {
    GENERATED_BODY()
  protected:
    virtual void BeginPlay() override;
  public:
    UFUNCTION()
    void OnOverlapBegin(AActor* OverlappedActor, AActor* other_actor);
  private:
    APlayerStart* player_start;
};
