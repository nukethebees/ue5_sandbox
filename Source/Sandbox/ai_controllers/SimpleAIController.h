#pragma once

#include "AIController.h"
#include "CoreMinimal.h"
#include "Sandbox/data/SimpleAIState.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "SimpleAIController.generated.h"

class UAIPerceptionComponent;
class UBehaviorTreeComponent;
class UBlackboardComponent;
class UBehaviorTree;
class UAISenseConfig_Sight;

UCLASS()
class SANDBOX_API ASimpleAIController
    : public AAIController
    , public ml::LogMsgMixin<"ASimpleAIController"> {
    GENERATED_BODY()
  public:
    ASimpleAIController();
  protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void on_target_perception_updated(AActor* Actor, FAIStimulus Stimulus);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UAIPerceptionComponent* ai_perception{nullptr};

    UPROPERTY(EditDefaultsOnly, Category = "AI")
    UBehaviorTreeComponent* behavior_tree_component{nullptr};

    UPROPERTY(EditDefaultsOnly, Category = "AI")
    UBlackboardComponent* blackboard_component{nullptr};

    UPROPERTY(EditDefaultsOnly, Category = "AI")
    UBehaviorTree* behavior_tree{nullptr};

    UPROPERTY(VisibleAnywhere, Category = "AI")
    UAISenseConfig_Sight* sight_config;
};
