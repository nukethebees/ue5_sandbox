#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/ValueOrBBKey.h"

#include "SandboxGameShared/logging/LogMsgMixin.hpp"
#include "ShooterGame/logging/ShooterGameLogCategories.h"

#include "BTTask_AttackEnemy.generated.h"

UCLASS()
class SHOOTERGAME_API UBTTask_AttackEnemy
    : public UBTTaskNode
    , public ml::LogMsgMixin<"UBTTask_AttackEnemy", LogShooterGameAI> {
    GENERATED_BODY()
  public:
    UBTTask_AttackEnemy();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
                                            uint8* NodeMemory) override;

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FValueOrBBKey_Object target_actor;
};
