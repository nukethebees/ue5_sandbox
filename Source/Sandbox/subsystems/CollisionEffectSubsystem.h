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
    void register_entity(AActor* component);
    static void try_register_entity(AActor* actor);
    void register_entity(UActorComponent* actor);
    static void try_register_entity(UActorComponent* component);
    void unregister_entity(UActorComponent* component);

    static UWorld* get_world_from_component(UActorComponent* component);
  private:
    TMap<TWeakObjectPtr<UPrimitiveComponent>, int32> collision_to_index{};
    TArray<TWeakObjectPtr<AActor>> collision_owners{};
    TArray<TArray<FEffectEntry>> effect_components{};

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
    bool is_valid_collision_entry(int32 index) const;
};
