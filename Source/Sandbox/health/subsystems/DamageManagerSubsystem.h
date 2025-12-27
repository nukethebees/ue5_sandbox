#pragma once

#include <cstddef>

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Sandbox/containers/LockFreeMPSCQueue.h"
#include "Sandbox/containers/MonitoredLockFreeMPSCQueue.h"
#include "Sandbox/health/data/HealthChange.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "DamageManagerSubsystem.generated.h"

class UHealthComponent;

USTRUCT()
struct FQueuedHealthChange {
    GENERATED_BODY()

    TWeakObjectPtr<UHealthComponent> Receiver;
    FHealthChange Change;
};

// Handles all health changes for a given tick using a lock-free queue
// Thread-safe: can be called from any thread including Mass parallel processors
UCLASS()
class SANDBOX_API UDamageManagerSubsystem
    : public UTickableWorldSubsystem
    , public ml::LogMsgMixin<"UDamageManagerSubsystem", LogSandboxSubsystem> {
    GENERATED_BODY()
  public:
    static constexpr std::size_t n_queue_elements{10000};

    void queue_health_change(UHealthComponent* receiver, FHealthChange const& change);
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return tick_enabled; }
    virtual TStatId GetStatId() const override;
  private:
    ml::MonitoredLockFreeMPSCQueue<ml::LockFreeMPSCQueue<FQueuedHealthChange>> damage_queue;
    bool tick_enabled{false};
};
