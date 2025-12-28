#pragma once

#include "CoreMinimal.h"
#include "AIController.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "SimpleAIController.generated.h"

class UAIPerceptionComponent;
class UBehaviorTree;
class UAISenseConfig_Sight;

UCLASS()
class SANDBOX_API ASimpleAIController
    : public AAIController
    , public ml::LogMsgMixin<"ASimpleAIController", LogSandboxAI> {
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
    UBehaviorTree* behaviour_tree_asset{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UAISenseConfig_Sight* sight_config;
};
