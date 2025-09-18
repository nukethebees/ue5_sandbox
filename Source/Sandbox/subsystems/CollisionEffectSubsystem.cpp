#include "Sandbox/subsystems/CollisionEffectSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "Sandbox/interfaces/CollisionEffectComponent.h"
#include "Sandbox/interfaces/CollisionOwner.h"
#include "Sandbox/subsystems/DestructionManagerSubsystem.h"

void UCollisionEffectSubsystem::register_actor(AActor* actor) {
    log_very_verbose(TEXT("Registering actor.\n"));

    auto* collision_owner{Cast<ICollisionOwner>(actor)};
    if (!collision_owner) {
        log_warning(TEXT("Actor doesn't implement ICollisionOwner.\n"));
        return;
    }

    auto* collision_comp{collision_owner->get_collision_component()};
    if (!collision_comp) {
        log_warning(TEXT("Actor doesn't have a collision component.\n"));
        return;
    }

    ensure_collision_registered(collision_comp, actor);
}
void UCollisionEffectSubsystem::register_effect_component(UActorComponent* component) {
    log_very_verbose(TEXT("Registering effect component.\n"));

    if (!component) {
        return;
    }

    auto* owner{component->GetOwner()};
    if (!owner) {
        return;
    }

    auto* collision_owner{Cast<ICollisionOwner>(owner)};
    if (!collision_owner) {
        log_warning(TEXT("Owner doesn't implement ICollisionOwner.\n"));
        return;
    }

    auto* collision_comp{collision_owner->get_collision_component()};
    if (!collision_comp) {
        log_warning(TEXT("Owner doesn't have a collision component.\n"));
        return;
    }

    ensure_collision_registered(collision_comp, owner);

    auto* effect_interface{Cast<ICollisionEffectComponent>(component)};
    if (!effect_interface) {
        return;
    }

    add_effect_to_collision(collision_comp, component);
}

void UCollisionEffectSubsystem::unregister_effect_component(UActorComponent* component) {
    if (!component) {
        return;
    }

    // Find and remove component from all effect arrays
    for (auto& effect_array : effect_components) {
        effect_array.RemoveAll(
            [component](FEffectEntry const& entry) { return entry.component.Get() == component; });
    }
}

void UCollisionEffectSubsystem::handle_collision_event(UPrimitiveComponent* OverlappedComponent,
                                                       AActor* OtherActor,
                                                       UPrimitiveComponent* OtherComp,
                                                       int32 OtherBodyIndex,
                                                       bool bFromSweep,
                                                       FHitResult const& SweepResult) {
    if (!OtherActor || !OverlappedComponent) {
        log_warning(TEXT("No OtherActor or OverlappedComponent."));
        return;
    }

    auto const* index_ptr{collision_to_index.Find(OverlappedComponent)};
    if (!index_ptr || !is_valid_collision_entry(*index_ptr)) {
        log_warning(TEXT("No collision entry."));
        return;
    }

    int32 const index{*index_ptr};
    auto* owner{collision_owners[index].Get()};
    if (!owner) {
        log_warning(TEXT("No owner."));
        return;
    }

    auto* collision_owner{Cast<ICollisionOwner>(owner)};
    if (!collision_owner) {
        log_warning(TEXT("Owner doesn't implement ICollisionOwner.\n"));
        return;
    }

    // Check cooldown
    static TMap<AActor*, double> cooldown_timers{};
    auto const current_time{GetWorld()->GetTimeSeconds()};
    auto const cooldown{collision_owner->get_collision_cooldown()};

    if (cooldown > 0.0f) {
        auto const last_collision{cooldown_timers.FindRef(owner)};
        if (current_time - last_collision < cooldown) {
            return;
        }
        cooldown_timers.Add(owner, current_time);
    }

    execute_effects_for_collision(owner, effect_components[index], OtherActor);

    // Handle destruction
    if (collision_owner->should_destroy_after_collision()) {
        if (auto* destruction_manager{GetWorld()->GetSubsystem<UDestructionManagerSubsystem>()}) {
            destruction_manager->queue_actor_destruction(owner);
        }
    }
}

void UCollisionEffectSubsystem::ensure_collision_registered(UPrimitiveComponent* collision_comp,
                                                            AActor* owner) {
    if (collision_to_index.Contains(collision_comp)) {
        return;
    }

    // Add new collision entry
    auto const index{collision_owners.Add(owner)};
    effect_components.AddDefaulted();
    collision_to_index.Add(collision_comp, index);

    // Bind overlap event
    collision_comp->OnComponentBeginOverlap.AddDynamic(
        this, &UCollisionEffectSubsystem::handle_collision_event);
}

void UCollisionEffectSubsystem::add_effect_to_collision(UPrimitiveComponent* collision_comp,
                                                        UActorComponent* component) {
    auto const* index_ptr{collision_to_index.Find(collision_comp)};
    if (!index_ptr) {
        return;
    }

    auto const index{*index_ptr};
    auto* effect_interface{Cast<ICollisionEffectComponent>(component)};
    if (!effect_interface) {
        return;
    }
    auto const priority{effect_interface->get_execution_priority()};

    auto& effects{effect_components[index]};
    effects.Emplace(component, effect_interface, priority);

    // Keep array sorted by priority
    effects.Sort();
}

void UCollisionEffectSubsystem::execute_effects_for_collision(AActor* owner,
                                                              TArray<FEffectEntry> const& effects,
                                                              AActor* other_actor) {
    if (!owner) {
        return;
    }

    auto* collision_owner{Cast<ICollisionOwner>(owner)};
    if (!collision_owner) {
        return;
    }

    // Pre-collision effect
    collision_owner->on_pre_collision_effect(other_actor);

    // Execute components in priority order (array is pre-sorted)
    for (auto const& entry : effects) {
        if (entry.component.IsValid() && entry.effect_interface) {
            entry.effect_interface->execute_effect(other_actor);
        }
    }

    // Main collision effect
    collision_owner->on_collision_effect(other_actor);

    // Post-collision effect
    collision_owner->on_post_collision_effect(other_actor);
}

void UCollisionEffectSubsystem::cleanup_invalid_entries() {
    // Remove invalid entries (called periodically or on demand)
    for (auto it{collision_to_index.CreateIterator()}; it; ++it) {
        if (!it->Key.IsValid()) {
            int32 const index{it->Value};
            if (index < collision_owners.Num()) {
                collision_owners[index] = nullptr;
                effect_components[index].Empty();
            }
            it.RemoveCurrent();
        }
    }
}

bool UCollisionEffectSubsystem::is_valid_collision_entry(int32 index) const {
    return index >= 0 && index < collision_owners.Num() && collision_owners[index].IsValid();
}
