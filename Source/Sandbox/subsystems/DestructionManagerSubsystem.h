#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"

#include "DestructionManagerSubsystem.generated.h"

// Handles delayed destruction of actors and components
// It doesn't tick by default
// When objects are queued for destruction, ticking is enabled
// The tick safely destroys all queued objects in a single loop
UCLASS()
class SANDBOX_API UDestructionManagerSubsystem : public UTickableWorldSubsystem {
    GENERATED_BODY()
  public:
    void queue_actor_destruction(AActor* actor);
    void queue_component_destruction(UActorComponent* component);
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return tick_enabled; }
    virtual TStatId GetStatId() const override;
  private:
    TArray<TWeakObjectPtr<AActor>> queued_actors;
    TArray<TWeakObjectPtr<UActorComponent>> queued_components;
    bool tick_enabled{false};

    void queue_destruction(auto* item, auto& queue) {
        if (!item) {
            return;
        }

        queue.Add(item);
        tick_enabled = true;
    }
};
