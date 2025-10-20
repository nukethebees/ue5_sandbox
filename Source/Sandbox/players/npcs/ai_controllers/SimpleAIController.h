#pragma once

#include "CoreMinimal.h"
#include "AIController.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/npcs/enums/SimpleAIState.h"

#include "SimpleAIController.generated.h"

class UAIPerceptionComponent;
class UBehaviorTreeComponent;
class UBlackboardComponent;
class UBehaviorTree;
class UAISenseConfig_Sight;

UCLASS()
class SANDBOX_API ASimpleAIController
    : public AAIController
    , public ml::LogMsgMixin<"ASimpleAIController", LogSandboxController> {
    GENERATED_BODY()
  public:
    ASimpleAIController();
  protected:
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

    UFUNCTION()
    void on_target_perception_updated(AActor* Actor, FAIStimulus Stimulus);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UAIPerceptionComponent* ai_perception{nullptr};

    UPROPERTY(EditDefaultsOnly, Category = "AI")
    UBehaviorTreeComponent* behavior_tree_component{nullptr};

    UPROPERTY(EditDefaultsOnly, Category = "AI")
    UBlackboardComponent* blackboard_component{nullptr};

    UPROPERTY(VisibleAnywhere, Category = "AI")
    UAISenseConfig_Sight* sight_config;
};
