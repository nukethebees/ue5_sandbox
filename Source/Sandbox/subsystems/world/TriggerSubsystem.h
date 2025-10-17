#pragma once

#include <utility>

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Sandbox/data/health/HealthStationPayload.h"
#include "Sandbox/data/trigger/ForcefieldPayload.h"
#include "Sandbox/data/trigger/RotatePayload.h"
#include "Sandbox/data/trigger/TriggerOtherPayload.h"
#include "Sandbox/data/trigger/WeaponPickupPayload.h"
#include "Sandbox/mixins/LogMsgMixin.hpp"
#include "Sandbox/mixins/TriggerSubsystemMixins.hpp"
#include "Sandbox/SandboxLogCategories.h"
#include "Sandbox/subsystems/world/TriggerSubsystemCore.h"

#include "TriggerSubsystem.generated.h"

UCLASS()
class SANDBOX_API UTriggerSubsystem
    : public UTickableWorldSubsystem
    , public UTriggerSubsystemMixins
    , public ml::LogMsgMixin<"UTriggerSubsystem", LogSandboxSubsystem> {
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
                         FWeaponPickupPayload,
                         FForcefieldPayload>
        core_{};
};
