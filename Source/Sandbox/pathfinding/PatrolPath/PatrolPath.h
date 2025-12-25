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
    virtual bool ShouldTickIfViewportsOnly() const { return true; }
#endif
    virtual void Tick(float dt) override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& event) override;
#endif
  protected:
#if WITH_EDITOR
    void draw_line_between_waypoints(UWorld& world, int32 a, int32 b);
#endif

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation|Debug")
    FColor debug_line_color{FColor::Blue};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation|Debug")
    float editor_line_arrow_size{5000.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation|Debug")
    float editor_line_thickness{20.0f};
#endif

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
    FName name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
    TArray<APatrolWaypoint*> waypoints;
};
