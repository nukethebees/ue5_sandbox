#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "CoreMinimal.h"

#include "BTTask_MoveToRandom.generated.h"

UCLASS()
class SANDBOX_API UBTTask_MoveToRandom : public UBTTaskNode {
    GENERATED_BODY()
  public:
    UBTTask_MoveToRandom();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
                                            uint8* NodeMemory) override;
  protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float radius{1000.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float acceptable_radius{5.0f};
};
