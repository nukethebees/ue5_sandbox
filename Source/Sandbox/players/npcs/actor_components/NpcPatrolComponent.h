#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "NpcPatrolComponent.generated.h"

class APatrolPath;
class APatrolWaypoint;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UNpcPatrolComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UNpcPatrolComponent();

    auto get_current_point() -> APatrolWaypoint*;
    auto get_current_point() const -> APatrolWaypoint*;
    auto get_previous_point() -> APatrolWaypoint*;
    auto get_previous_point() const -> APatrolWaypoint*;

    void advance_to_next_point();
    void set_arrived_at_point();
    auto arrived_at_point() const -> bool;
  protected:
    virtual void BeginPlay() override;

    void set_current_waypoint(APatrolWaypoint* wp);
    void set_waypoint_to_default();

    UPROPERTY(EditAnywhere, Category = "Patrol")
    APatrolPath* patrol_path{nullptr};
    UPROPERTY(EditAnywhere, Category = "Patrol")
    APatrolWaypoint* last_waypoint{nullptr};
    UPROPERTY(EditAnywhere, Category = "Patrol")
    APatrolWaypoint* current_waypoint{nullptr};
};
