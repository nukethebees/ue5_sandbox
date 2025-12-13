// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "PatrolPath.generated.h"

class APatrolWaypoint;

UCLASS()
class SANDBOX_API APatrolPath : public AActor {
    GENERATED_BODY()
  public:
    APatrolPath();

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& event) override;
#endif
  protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
    FName name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
    TArray<APatrolWaypoint*> waypoints;
};
