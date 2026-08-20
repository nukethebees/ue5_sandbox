#pragma once

#include "ShooterGame/combat/weapons/WeaponPickupPayload.h"
#include "Sandbox/environment/effects/RotatePayload.h"
#include "ShooterGame/environment/obstacles/ForcefieldPayload.h"
#include "ShooterGame/health/HealthStationPayload.h"
#include "ShooterGame/interaction/TriggerOtherPayload.h"
#include "ShooterGame/interaction/TriggerSubsystemCore.h"
#include "ShooterGame/interaction/TriggerSubsystemMixins.hpp"
#include "ShooterGame/items/MoneyPickupPayload.h"
#include "SandboxGameShared/logging/LogMsgMixin.hpp"
#include "ShooterGame/logging/ShooterGameLogCategories.h"

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include <utility>

#include "TriggerSubsystem.generated.h"

UCLASS()
class SHOOTERGAME_API UTriggerSubsystem
    : public UTickableWorldSubsystem
    , public UTriggerSubsystemMixins
    , public ml::LogMsgMixin<"UTriggerSubsystem", LogShooterGameSubsystem> {
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
