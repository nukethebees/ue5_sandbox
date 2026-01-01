#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/ValueOrBBKey.h"

#include "BTTask_PerformLookPattern.generated.h"

class APawn;

UENUM()
enum class ELookPatternMoveState : uint8 { GoingRight, GoingLeft, ReturnToCentre };

UCLASS()
class SANDBOX_API UBTTask_PerformLookPattern : public UBTTaskNode {
    GENERATED_BODY()
  public:
    UBTTask_PerformLookPattern();

    virtual auto ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
        -> EBTNodeResult::Type override;
    virtual void
        TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
  protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    FValueOrBBKey_Float half_angle_degrees{90.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    FValueOrBBKey_Float interp_speed{3.0f};
  private:
    auto get_target_rotation(ELookPatternMoveState state) -> FQuat;
    void set_new_target(FQuat new_tgt);

    ELookPatternMoveState move_state{ELookPatternMoveState::GoingRight};
    FQuat actor_fwd{FQuat::Identity};
    FQuat actor_right{FQuat::Identity};
    FQuat actor_left{FQuat::Identity};
    FQuat target{FQuat::Identity};
    FQuat start_rotation{FQuat::Identity};
    float progress{0.f};
    APawn* pawn{nullptr};
};
