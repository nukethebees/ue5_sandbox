#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Sandbox/environment/effects/enums/RotationType.h"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "RotationManagerSubsystem.generated.h"

class URotatingActorComponent;

// Handles all rotations for a given tick using struct of arrays
// It doesn't tick by default
// When rotating components register, the ticking is enabled
// The tick applies rotations to all registered actors in a single loop
// Dynamic rotations: can change speed, components remain alive
// Static rotations: fixed speed, components can be destroyed after registration
UCLASS()
class SANDBOX_API URotationManagerSubsystem
    : public UTickableWorldSubsystem
    , public ml::LogMsgMixin<"URotationManagerSubsystem", LogSandboxSubsystem> {
    GENERATED_BODY()
  public:
    void add(USceneComponent& scene_component, URotatingActorComponent& component);
    void add(USceneComponent& scene_component, float speed);
    void remove(URotatingActorComponent& component);
    void remove(USceneComponent& component);

    virtual bool IsTickable() const override { return tick_enabled; }
    virtual TStatId GetStatId() const override;
  protected:
    virtual void Tick(float DeltaTime) override;
  private:
    void update_tick_enabled() {
        tick_enabled = !dynamic_components.IsEmpty() || !static_scene_components.IsEmpty();
    }

    // Dynamic rotation arrays - components can change speed
    TArray<TWeakObjectPtr<USceneComponent>> dynamic_scene_components;
    TArray<URotatingActorComponent*> dynamic_components;

    // Static rotation arrays - fixed speed, components may be destroyed
    TArray<TWeakObjectPtr<USceneComponent>> static_scene_components;
    TArray<float> static_speeds;

    bool tick_enabled{false};
};
