#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sandbox/data/trigger/TriggerOtherPayload.h"
#include "Sandbox/data/trigger/TriggerableId.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "TriggerButtonActor.generated.h"

class USceneComponent;
class UTriggerSubsystem;

UCLASS(BlueprintType, Blueprintable)
class SANDBOX_API ATriggerButtonActor
    : public AActor
    , public ml::LogMsgMixin<"ATriggerButtonActor"> {
    GENERATED_BODY()
  public:
    ATriggerButtonActor();

    // Blueprint-editable array of target actors
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Button")
    TArray<AActor*> target_actors;

    // Activation delay before triggering targets
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Button")
    float activation_delay{0.0f};

    UFUNCTION(BlueprintCallable, Category = "Trigger Button")
    int32 get_target_count() const { return trigger_payload.n_targets; }

    UFUNCTION(BlueprintCallable, Category = "Trigger Button")
    bool has_valid_targets() const { return trigger_payload.n_targets > 0; }
  protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type reason) override;
  private:
    FTriggerOtherPayload trigger_payload{};
    TriggerableId my_trigger_id{};

    // Convert actor pointers to TriggerableIds and populate payload
    void register_targets_with_payload();
};
