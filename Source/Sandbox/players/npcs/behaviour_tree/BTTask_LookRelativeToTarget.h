#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/ValueOrBBKey.h"

#include "BTTask_LookRelativeToTarget.generated.h"

UCLASS()
class SANDBOX_API UBTTask_LookRelativeToTarget : public UBTTaskNode {
    GENERATED_BODY()
  public:
    UBTTask_LookRelativeToTarget();

    virtual auto ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
        -> EBTNodeResult::Type override;
    virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
  protected:
    UPROPERTY(EditAnywhere)
    FBlackboardKeySelector target_key;
    UPROPERTY(EditAnywhere)
    FValueOrBBKey_Float angle_deg{180.0f};
};
