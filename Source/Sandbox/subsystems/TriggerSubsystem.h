#pragma once

#include <utility>

#include "CoreMinimal.h"
#include "Sandbox/data/health/HealthStationPayload.h"
#include "Sandbox/data/trigger/FForcefieldPayload.h"
#include "Sandbox/data/trigger/FRotatePayload.h"
#include "Sandbox/data/trigger/TriggerOtherPayload.h"
#include "Sandbox/subsystems/TriggerSubsystemCore.h"
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
    virtual void Tick(float DeltaTime) override { core_.tick_payloads(DeltaTime); }
    virtual bool IsTickable() const override { return core_.has_ticking_payloads(); }
    virtual TStatId GetStatId() const override { return TStatId(); }
    virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
  private:
    TriggerSubsystemCore<FTriggerOtherPayload,
                          FHealthStationPayload,
                          FRotatePayload,
                          FForcefieldPayload>
        core_{};
};
