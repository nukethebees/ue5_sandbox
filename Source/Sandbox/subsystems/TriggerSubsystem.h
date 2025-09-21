#pragma once

#include "CoreMinimal.h"
#include "Sandbox/data/TriggerOtherPayload.h"
#include "Sandbox/data/TriggerSubsystemData.h"
#include "Subsystems/WorldSubsystem.h"

#include "TriggerSubsystem.generated.h"

UCLASS()
class SANDBOX_API UTriggerSubsystem : public UWorldSubsystem {
    GENERATED_BODY()
  public:
    template <typename T>
    auto register_triggerable(AActor* actor, T&& payload) {
        return data_.register_triggerable(actor, std::forward<T>(payload), this);
    }

    void trigger(TriggerableId id, FTriggeringSource source) { data_.trigger(id, source); }

    void deregister_triggerable(AActor* actor) { data_.deregister_triggerable(actor); }

    auto get_triggerable_id(AActor* actor) { return data_.get_triggerable_id(actor); }
  private:
    UTriggerSubsystemData<FTriggerOtherPayload> data_{};
};
