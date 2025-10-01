#pragma once

#include "AIController.h"
#include "CoreMinimal.h"
#include "Sandbox/data/SimpleAIState.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "SimpleManualAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;

UENUM(BlueprintType)
enum class ESimpleManualAIState : uint8 {
    Idle UMETA(DisplayName = "Idle"),
    Wandering UMETA(DisplayName = "Wandering"),
    Following UMETA(DisplayName = "Following"),
    Stuck UMETA(DisplayName = "Stuck")
};

USTRUCT(BlueprintType)
struct FSimpleManualAIControllerConfig {
    GENERATED_BODY()

    // Wandering
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float wander_time{2.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float wander_wait_time{2.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float wander_radius{1000.0f};

    // General moving
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float acceptable_move_radius{5.0f};
};

USTRUCT()
struct FSimpleManualAIControllerMemory {
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere)
    ESimpleManualAIState state{ESimpleManualAIState::Idle};

    UPROPERTY(VisibleAnywhere)
    ESimpleManualAIState next_state{ESimpleManualAIState::Idle};

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
    virtual void Tick(float delta_time) override;

    UFUNCTION()
    void on_target_perception_updated(AActor* Actor, FAIStimulus Stimulus);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UAIPerceptionComponent* ai_perception{nullptr};

    UPROPERTY(VisibleAnywhere, Category = "AI")
    UAISenseConfig_Sight* sight_config;

    UPROPERTY(VisibleAnywhere, Category = "AI")
    FSimpleManualAIControllerConfig config;

    UPROPERTY(VisibleAnywhere, Category = "AI")
    FSimpleManualAIControllerMemory memory;

    void update_fsm(float delta_time);
    void move_to_state(ESimpleManualAIState new_state);
    void fsm_idle(float delta_time);
    void fsm_wandering(float delta_time);
    void fsm_following(float delta_time);
    void fsm_stuck(float delta_time);
  private:
    FTimerHandle wander_timer;
};
