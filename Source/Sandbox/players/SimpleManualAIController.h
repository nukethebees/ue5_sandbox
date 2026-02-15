#pragma once

#include <optional>
#include <utility>

#include "CoreMinimal.h"
#include "AIController.h"
#include "Misc/Optional.h"
#include "TimerManager.h"

#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/players/SimpleAIState.h"

#include "SimpleManualAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;

UENUM(BlueprintType)
enum class ESimpleManualAIState : uint8 {
    Idle UMETA(DisplayName = "Idle"),
    Wandering UMETA(DisplayName = "Wandering"),
    Following UMETA(DisplayName = "Following"),
    Moving UMETA(DisplayName = "Moving"),
    MovingToLastKnownLocation UMETA(DisplayName = "MovingToLastKnownLocation"),
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

    void print_states() const;

    UPROPERTY(VisibleAnywhere)
    ESimpleManualAIState previous_state{ESimpleManualAIState::Idle};

    UPROPERTY(VisibleAnywhere)
    ESimpleManualAIState state{ESimpleManualAIState::Idle};

    UPROPERTY(VisibleAnywhere)
    ESimpleManualAIState next_state{ESimpleManualAIState::Idle};

    UPROPERTY(VisibleAnywhere)
    AActor* target{nullptr};

    UPROPERTY(VisibleAnywhere)
    TOptional<FVector> last_known_location{NullOpt};
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

    virtual void OnMoveCompleted(FAIRequestID RequestID,
                                 FPathFollowingResult const& Result) override;

    void update_fsm(float delta_time);
    void move_to_state(ESimpleManualAIState new_state);
    void move_to_next_state();

    void fsm_idle(float delta_time);
    void fsm_wandering(float delta_time);
    void fsm_following(float delta_time);
    void fsm_moving(float delta_time);
    void fsm_moving_to_last_known_location(float delta_time);
    void fsm_stuck(float delta_time);

    void wait_for_time(auto&& callback, float wait_time) {
        constexpr bool repeat{false};
        log_verbose(TEXT("Waiting for %f seconds."), wait_time);
        GetWorld()->GetTimerManager().SetTimer(
            wait_timer, std::forward<decltype(callback)>(callback), wait_time, repeat);
    }
    void move_to_location(FVector location);
    void handle_successful_move();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UAIPerceptionComponent* ai_perception{nullptr};

    UPROPERTY(VisibleAnywhere, Category = "AI")
    UAISenseConfig_Sight* sight_config;

    UPROPERTY(VisibleAnywhere, Category = "AI")
    FSimpleManualAIControllerConfig config;

    UPROPERTY(VisibleAnywhere, Category = "AI")
    FSimpleManualAIControllerMemory memory;
  private:
    FTimerHandle wait_timer;
};
