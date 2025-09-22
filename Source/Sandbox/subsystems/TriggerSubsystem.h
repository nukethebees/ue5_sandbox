#pragma once

#include <utility>

#include "CoreMinimal.h"
#include "Sandbox/data/health/HealthStationPayload.h"
#include "Sandbox/data/trigger/FForcefieldPayload.h"
#include "Sandbox/data/trigger/FRotatePayload.h"
#include "Sandbox/data/trigger/TriggerOtherPayload.h"
#include "Sandbox/data/trigger/TriggerSubsystemData.h"
#include "Sandbox/mixins/TriggerSubsystemMixins.hpp"
#include "Subsystems/WorldSubsystem.h"

#include "TriggerSubsystem.generated.h"

UCLASS()
class SANDBOX_API UTriggerSubsystem
    : public UTickableWorldSubsystem
    , public UTriggerSubsystemMixins {
    GENERATED_BODY()
    friend class UTriggerSubsystemMixins;
  public:
    virtual void Tick(float DeltaTime) override { data_.tick_payloads(DeltaTime); }
    virtual bool IsTickable() const override { return data_.has_ticking_payloads(); }
    virtual TStatId GetStatId() const override { return TStatId(); }
    virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
  private:
    UTriggerSubsystemData<FTriggerOtherPayload,
                          FHealthStationPayload,
                          FRotatePayload,
                          FForcefieldPayload>
        data_{};
};
