#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "CollisionEffectSubsystem.generated.h"

class ICollisionEffectComponent;
class ICollisionOwner;

struct FEffectEntry {
    TWeakObjectPtr<UActorComponent> component{};
    ICollisionEffectComponent* effect_interface{};
    int32 priority{0};

    FEffectEntry() = default;
    FEffectEntry(UActorComponent* comp, ICollisionEffectComponent* effect, int32 prio)
        : component(comp)
        , effect_interface(effect)
        , priority(prio) {}

    // For sorting by priority (lower values = earlier execution)
    bool operator<(FEffectEntry const& other) const { return priority < other.priority; }
};

namespace ml {
inline static constexpr wchar_t UCollisionEffectSubsystemLogTag[]{
    TEXT("UCollisionEffectSubsystem")};
}

UCLASS()
class SANDBOX_API UCollisionEffectSubsystem
    : public UWorldSubsystem
    , public ml::LogMsgMixin<ml::UCollisionEffectSubsystemLogTag> {
    GENERATED_BODY()
  public:
    void register_actor(AActor* component);
    void register_effect_component(UActorComponent* actor);
    void unregister_effect_component(UActorComponent* component);

    // Utility functions ported from CollisionEffectHelpers
    static UWorld* get_world_from_component(UActorComponent* component);
    static UWorld* get_world_from_actor(AActor* actor);

    // Convenience registration functions
    static void register_entity(UActorComponent* component);
    static void register_entity(AActor* actor);
  private:
    // Optimized storage for fast collision event processing
    TMap<TWeakObjectPtr<UPrimitiveComponent>, int32> collision_to_index{};
    TArray<TWeakObjectPtr<AActor>> collision_owners{};
    TArray<TArray<FEffectEntry>> effect_components{};

    // Collision event handler
    UFUNCTION()
    void handle_collision_event(UPrimitiveComponent* OverlappedComponent,
                                AActor* OtherActor,
                                UPrimitiveComponent* OtherComp,
                                int32 OtherBodyIndex,
                                bool bFromSweep,
                                FHitResult const& SweepResult);

    // Internal registration helpers
    void ensure_collision_registered(UPrimitiveComponent* collision_comp, AActor* owner);
    void add_effect_to_collision(UPrimitiveComponent* collision_comp, UActorComponent* component);
    void execute_effects_for_collision(AActor* owner,
                                       TArray<FEffectEntry> const& effects,
                                       AActor* other_actor);

    // Cleanup helpers
    void cleanup_invalid_entries();
    bool is_valid_collision_entry(int32 index) const;
};
