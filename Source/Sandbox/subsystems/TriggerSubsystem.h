#pragma once

#include <utility>

#include "CoreMinimal.h"
#include "Sandbox/data/HealthStationPayload.h"
#include "Sandbox/data/TriggerOtherPayload.h"
#include "Sandbox/data/TriggerResults.h"
#include "Sandbox/data/TriggerSubsystemData.h"
#include "Subsystems/WorldSubsystem.h"

#include "TriggerSubsystem.generated.h"

UCLASS()
class SANDBOX_API UTriggerSubsystem
    : public UWorldSubsystem
    , public FTickableGameObject {
    GENERATED_BODY()
  public:
    template <typename T>
    auto register_triggerable(AActor* actor, T&& payload) {
        return data_.register_triggerable(actor, std::forward<T>(payload), this);
    }

    template <typename... Args>
    auto trigger(Args&&... args) {
        return data_.trigger(std::forward<Args>(args)...);
    }

    void deregister_triggerable(AActor* actor) { data_.deregister_triggerable(actor); }

    auto get_triggerable_id(AActor* actor) { return data_.get_triggerable_id(actor); }

    // FTickableGameObject interface
    virtual void Tick(float DeltaTime) override { data_.tick_payloads(DeltaTime); }
    virtual bool IsTickable() const override { return data_.has_ticking_payloads(); }
    virtual TStatId GetStatId() const override { return TStatId(); }
    virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
  private:
    UTriggerSubsystemData<FTriggerOtherPayload, FHealthStationPayload> data_{};
};
