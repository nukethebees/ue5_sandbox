#pragma once

#include "Sandbox/combat/weapons/WeaponPickupPayload.h"
#include "Sandbox/environment/effects/data/RotatePayload.h"
#include "Sandbox/environment/obstacles/data/ForcefieldPayload.h"
#include "Sandbox/health/HealthStationPayload.h"
#include "Sandbox/interaction/TriggerOtherPayload.h"
#include "Sandbox/interaction/TriggerSubsystemCore.h"
#include "Sandbox/interaction/TriggerSubsystemMixins.hpp"
#include "Sandbox/items/MoneyPickupPayload.h"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include <utility>

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
                         FForcefieldPayload,
                         FMoneyPickupPayload>
        core_{};
};
