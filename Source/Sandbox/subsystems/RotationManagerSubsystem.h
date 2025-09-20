#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Sandbox/data/RotationType.h"

#include "RotationManagerSubsystem.generated.h"

class URotatingActorComponent;

// Handles all rotations for a given tick using struct of arrays
// It doesn't tick by default
// When rotating components register, the ticking is enabled
// The tick applies rotations to all registered actors in a single loop
// Dynamic rotations: can change speed, components remain alive
// Static rotations: fixed speed, components can be destroyed after registration
UCLASS()
class SANDBOX_API URotationManagerSubsystem : public UTickableWorldSubsystem {
    GENERATED_BODY()
  public:
    void
        add_dynamic(URotatingActorComponent* component, AActor* actor, ERotationType rotation_type);
    void add_static(float speed, AActor* actor);
    void remove(URotatingActorComponent* component);
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return tick_enabled; }
    virtual TStatId GetStatId() const override;
  private:
    // Dynamic rotation arrays - components can change speed
    TArray<URotatingActorComponent*> dynamic_components;
    TArray<TWeakObjectPtr<AActor>> dynamic_actors;

    // Static rotation arrays - fixed speed, components may be destroyed
    TArray<TWeakObjectPtr<AActor>> static_actors;
    TArray<float> static_speeds;

    bool tick_enabled{false};
};
