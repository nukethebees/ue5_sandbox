#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Sandbox/data/trigger/RotatePayload.h"
#include "Sandbox/data/trigger/TriggerableId.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "InteractRotateComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UInteractRotateComponent
    : public UActorComponent
    , public ml::LogMsgMixin<"UInteractRotateComponent"> {
    GENERATED_BODY()
  public:
    UInteractRotateComponent();
  protected:
    virtual void BeginPlay() override;
  private:
    UPROPERTY(EditAnywhere, Category = "Rotation")
    FRotatePayload rotation_payload{};
};
