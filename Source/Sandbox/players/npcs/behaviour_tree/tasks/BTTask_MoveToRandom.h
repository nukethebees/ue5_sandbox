#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/ValueOrBBKey.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "BTTask_MoveToRandom.generated.h"

UCLASS()
class SANDBOX_API UBTTask_MoveToRandom
    : public UBTTaskNode
    , public ml::LogMsgMixin<"UBTTask_MoveToRandom", LogSandboxAI> {
    GENERATED_BODY()
  public:
    UBTTask_MoveToRandom();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
                                            uint8* NodeMemory) override;
  protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    FValueOrBBKey_Float radius{1000.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    FValueOrBBKey_Float acceptable_radius{5.0f};
};
