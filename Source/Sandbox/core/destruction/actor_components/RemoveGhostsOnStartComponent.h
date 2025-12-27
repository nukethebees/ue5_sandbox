// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "RemoveGhostsOnStartComponent.generated.h"

// Upon first tick, go through the level and destroy every actor component
// that holds cleanup_tag
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API URemoveGhostsOnStartComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    URemoveGhostsOnStartComponent();
  public:
    virtual void TickComponent(float DeltaTime,
                               ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cleanup")
    FName cleanup_tag{"KillOnFirstTick"};
};
