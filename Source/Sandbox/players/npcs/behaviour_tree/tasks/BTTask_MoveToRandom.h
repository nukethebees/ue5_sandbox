#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/ValueOrBBKey.h"
#include "Navigation/PathFollowingComponent.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "BTTask_MoveToRandom.generated.h"

struct FPathFollowingResult;

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
    UFUNCTION()
    void on_move_completed(FAIRequestID RequestID, EPathFollowingResult::Type Result);

    UFUNCTION()
    void on_enemy_spotted();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    FValueOrBBKey_Float travel_radius{1000.0f};
    // The threshold for "arriving"
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    FValueOrBBKey_Float acceptable_radius{5.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    FValueOrBBKey_Bool interrupt_for_enemy{true};
};
