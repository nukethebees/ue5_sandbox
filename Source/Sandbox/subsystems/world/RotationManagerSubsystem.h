#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Sandbox/enums/RotationType.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/SandboxLogCategories.h"

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
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return tick_enabled; }
    virtual TStatId GetStatId() const override;
  private:
    // Dynamic rotation arrays - components can change speed
    TArray<TWeakObjectPtr<USceneComponent>> dynamic_scene_components;
    TArray<URotatingActorComponent*> dynamic_components;

    // Static rotation arrays - fixed speed, components may be destroyed
    TArray<TWeakObjectPtr<USceneComponent>> static_scene_components;
    TArray<float> static_speeds;

    bool tick_enabled{false};
};
