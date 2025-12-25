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

    virtual void Tick(float dt) override;

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
    virtual void PostEditChangeProperty(FPropertyChangedEvent& event) override;
#endif
  protected:
#if WITH_EDITOR
    void draw_line_between_waypoints(UWorld& world, int32 a, int32 b);
#endif

#if WITH_EDITORONLY_DATA
    static int32 color_index;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation|Editor Visualisation")
    FColor editor_line_color{FColor::Blue};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation|Editor Visualisation")
    float editor_line_arrow_size{5000.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation|Editor Visualisation")
    float editor_line_thickness{20.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation|Editor Visualisation")
    bool editor_line_require_selection{false};
#endif

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
    FName name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
    TArray<APatrolWaypoint*> waypoints;
};
