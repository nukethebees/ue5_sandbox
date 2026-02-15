#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/ValueOrBBKey.h"

#include "BTTask_GetRandomPoint.generated.h"

UCLASS()
class SANDBOX_API UBTTask_GetRandomPoint : public UBTTaskNode {
    GENERATED_BODY()
  public:
    UBTTask_GetRandomPoint();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
                                            uint8* NodeMemory) override;
  protected:
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector target_location;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackboard")
    FValueOrBBKey_Float travel_radius{1000.0f};
};
