#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"

#include "DestructionManagerSubsystem.generated.h"

// Handles delayed destruction of actors and components
// It doesn't tick by default
// When objects are queued for destruction, ticking is enabled
// The tick safely destroys all queued objects in a single loop
UCLASS()
class SANDBOX_API UDestructionManagerSubsystem
    : public UTickableWorldSubsystem
    , public ml::LogMsgMixin<"UDestructionManagerSubsystem"> {
    GENERATED_BODY()
  public:
    void queue_destruction(AActor* actor);
    void queue_destruction(UActorComponent* component);
    void queue_destruction_with_delay(AActor* actor, float delay);
    void queue_destruction_with_delay(UActorComponent* component, float delay);
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return tick_enabled; }
    virtual TStatId GetStatId() const override;
  private:
    TArray<TWeakObjectPtr<AActor>> queued_actors;
    TArray<TWeakObjectPtr<UActorComponent>> queued_components;
    TMap<int32, TWeakObjectPtr<AActor>> delayed_actors;
    TMap<int32, TWeakObjectPtr<UActorComponent>> delayed_components;
    TMap<int32, FTimerHandle> active_timers;
    int32 next_destruction_id{1};
    bool tick_enabled{false};

    void queue_destruction(auto* item, auto& queue) {
        if (!item) {
            return;
        }

        queue.Add(item);
        tick_enabled = true;
    }

    void destroy_actor_delayed(int32 destruction_id);
    void destroy_component_delayed(int32 destruction_id);
};
