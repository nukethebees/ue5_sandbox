#pragma once

#include "AIController.h"
#include "CoreMinimal.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "SimpleAIController.generated.h"

UENUM(BlueprintType)
enum class ESimpleAIState : uint8 {
    Wandering UMETA(DisplayName = "Wandering"),
    Following UMETA(DisplayName = "Following"),
    Investigating UMETA(DisplayName = "Investigating")
};

class UAIPerceptionComponent;
class UBehaviorTreeComponent;
class UBlackboardComponent;
class UBehaviorTree;

UCLASS()
class SANDBOX_API ASimpleAIController
    : public AAIController
    , public ml::LogMsgMixin<"ASimpleAIController"> {
    GENERATED_BODY()
  public:
    ASimpleAIController();
  protected:
    virtual void BeginPlay() override;
    virtual void OnMoveCompleted(FAIRequestID RequestID,
                                 FPathFollowingResult const& Result) override;

    void choose_new_target();

    UPROPERTY(EditAnywhere, Category = "AI")
    float wander_radius{1000.0f};

    UPROPERTY(EditAnywhere, Category = "AI")
    float wander_interval{3.0f};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UAIPerceptionComponent* ai_perception{nullptr};

    UPROPERTY(EditDefaultsOnly, Category = "AI")
    UBehaviorTreeComponent* behavior_tree_component{nullptr};

    UPROPERTY(EditDefaultsOnly, Category = "AI")
    UBlackboardComponent* blackboard_component{nullptr};

    UPROPERTY(EditDefaultsOnly, Category = "AI")
    UBehaviorTree* behavior_tree{nullptr};
  private:
    FTimerHandle wander_timer;
};
