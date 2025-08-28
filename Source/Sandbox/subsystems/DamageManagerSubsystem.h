#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Sandbox/actor_components/HealthComponent.h"
#include "Sandbox/actor_components/HealthChange.h"

#include "DamageManagerSubsystem.generated.h"

USTRUCT()
struct FQueuedHealthChange {
    GENERATED_BODY()

    TWeakObjectPtr<UHealthComponent> Receiver;
    FHealthChange Change;
};

// Handles all health changes for a given tick
// It doesn't tick by default
// When damage dealing actors apply damage, the ticking is enabled
// The tick clears the queue of health changes in a single loop
UCLASS()
class SANDBOX_API UDamageManagerSubsystem : public UTickableWorldSubsystem {
    GENERATED_BODY()
  public:
    void QueueDamage(UHealthComponent* receiver, FHealthChange const& change);
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return tick_enabled; }
  private:
    TArray<FQueuedHealthChange> PendingChanges;
    bool tick_enabled{false};
};
