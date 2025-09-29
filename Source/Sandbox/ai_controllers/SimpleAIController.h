#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "SimpleAIController.generated.h"

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
  private:
    FTimerHandle wander_timer;
};
