#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sandbox/environment/effects/data/RotatePayload.h"
#include "Sandbox/interaction/triggering/data/TriggerableId.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"

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
