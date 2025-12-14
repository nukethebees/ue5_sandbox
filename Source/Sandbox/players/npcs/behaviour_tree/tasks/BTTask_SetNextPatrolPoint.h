#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BTTaskNode.h"

#include "BTTask_SetNextPatrolPoint.generated.h"

struct FPathFollowingResult;

UCLASS()
class SANDBOX_API UBTTask_SetNextPatrolPoint : public UBTTaskNode {
    GENERATED_BODY()
  public:
    UBTTask_SetNextPatrolPoint();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
                                            uint8* NodeMemory) override;
  protected:
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector move_destination;
};
