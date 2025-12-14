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

    auto get_first_point() -> APatrolWaypoint* {
        if (waypoints.IsEmpty()) {
            return nullptr;
        }
        return waypoints[0];
    }
    auto operator[](int32 i) -> APatrolWaypoint* {
        return waypoints.IsValidIndex(i) ? waypoints[i] : nullptr;
    }
    auto length() const -> int32 { return waypoints.Num(); }
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& event) override;
#endif
  protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
    FName name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
    TArray<APatrolWaypoint*> waypoints;
};
