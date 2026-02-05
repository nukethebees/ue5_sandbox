#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BTTaskNode.h"

#include "BTTask_ClearBbValue.generated.h"

UCLASS()
class SANDBOX_API UBTTask_ClearBbValue : public UBTTaskNode {
    GENERATED_BODY()
  public:
    UBTTask_ClearBbValue();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
                                            uint8* NodeMemory) override;
  protected:
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector bb_key;
};
