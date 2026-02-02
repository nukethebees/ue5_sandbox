#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/ValueOrBBKey.h"

#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "BTTask_AttackEnemy.generated.h"

UCLASS()
class SANDBOX_API UBTTask_AttackEnemy
    : public UBTTaskNode
    , public ml::LogMsgMixin<"UBTTask_AttackEnemy", LogSandboxAI> {
    GENERATED_BODY()
  public:
    UBTTask_AttackEnemy();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
                                            uint8* NodeMemory) override;

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FValueOrBBKey_Object target_actor;
};
