#pragma once

#include "AIController.h"
#include "CoreMinimal.h"
#include "Sandbox/data/SimpleAIState.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "SimpleManualAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;

USTRUCT()
struct FSimpleManualAIControllerMemory {
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere)
    AActor* target{nullptr};
};

UCLASS()
class SANDBOX_API ASimpleManualAIController
    : public AAIController
    , public ml::LogMsgMixin<"ASimpleManualAIController"> {
    GENERATED_BODY()
  public:
    ASimpleManualAIController();
  protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void on_target_perception_updated(AActor* Actor, FAIStimulus Stimulus);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UAIPerceptionComponent* ai_perception{nullptr};

    UPROPERTY(VisibleAnywhere, Category = "AI")
    UAISenseConfig_Sight* sight_config;

    UPROPERTY(VisibleAnywhere, Category = "AI")
    FSimpleManualAIControllerMemory memory;
  private:
    FTimerHandle wander_timer;
};
