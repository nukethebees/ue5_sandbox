#include "Sandbox/subsystems/CollisionEffectSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "Sandbox/interfaces/CollisionEffectComponent.h"
#include "Sandbox/interfaces/CollisionOwner.h"
#include "Sandbox/subsystems/DestructionManagerSubsystem.h"

void UCollisionEffectSubsystem::register_entity(AActor& actor) {
    log_very_verbose(TEXT("Registering actor.\n"));

    auto* collision_owner{Cast<ICollisionOwner>(&actor)};
    if (!collision_owner) {
        log_warning(TEXT("Actor doesn't implement ICollisionOwner.\n"));
        return;
    }

    auto* collision_comp{collision_owner->get_collision_component()};
    if (!collision_comp) {
        log_warning(TEXT("Actor doesn't have a collision component.\n"));
        return;
    }

    register_collision_box(*collision_comp, actor);
}
void UCollisionEffectSubsystem::try_register_entity(AActor* actor) {
    if (!actor) {
        return;
    }

    if (auto* world{actor->GetWorld()}) {
        if (auto* subsystem{world->GetSubsystem<UCollisionEffectSubsystem>()}) {
            subsystem->register_entity(*actor);
        }
    }
}
void UCollisionEffectSubsystem::register_entity(UActorComponent& component) {
    log_very_verbose(TEXT("Registering effect component.\n"));

    auto* owner{component.GetOwner()};
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

    auto const i{register_collision_box(*collision_comp, *owner)};
    add_effect_to_collision(i, component);
}
void UCollisionEffectSubsystem::try_register_entity(UActorComponent* component) {
    if (!component) {
        return;
    }

    if (auto* world{get_world_from_component(component)}) {
        if (auto* subsystem{world->GetSubsystem<UCollisionEffectSubsystem>()}) {
            subsystem->register_entity(*component);
        }
    }
}

void UCollisionEffectSubsystem::unregister_entity(UActorComponent* component) {
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

    // Should have already been validated
    auto& collision_owner{*Cast<ICollisionOwner>(owner)};

    static TMap<AActor*, double> cooldown_timers{};
    auto const current_time{GetWorld()->GetTimeSeconds()};
    auto const cooldown{collision_owner.get_collision_cooldown()};

    if (cooldown > 0.0f) {
        auto const last_collision{cooldown_timers.FindRef(owner)};
        auto const time_since_last_collision{current_time - last_collision};
        if (time_since_last_collision < cooldown) {
            return;
        }
        cooldown_timers.Add(owner, current_time);
    }

    // Execute the effects
    collision_owner.on_pre_collision_effect(OtherActor);
    for (auto const& entry : effect_components[index]) {
        if (entry.component.IsValid() && entry.effect_interface) {
            entry.effect_interface->execute_effect(OtherActor);
        }
    }
    collision_owner.on_collision_effect(OtherActor);
    collision_owner.on_post_collision_effect(OtherActor);

    if (collision_owner.should_destroy_after_collision()) {
        if (auto* destruction_manager{GetWorld()->GetSubsystem<UDestructionManagerSubsystem>()}) {
            destruction_manager->queue_actor_destruction(owner);
        }
    }
}

int32 UCollisionEffectSubsystem::register_collision_box(UPrimitiveComponent& collision_comp,
                                                        AActor& owner) {
    if (auto idx{collision_to_index.Find(&collision_comp)}) {
        return *idx;
    }

    auto const index{collision_owners.Add(&owner)};
    effect_components.AddDefaulted();
    collision_to_index.Add(&collision_comp, index);

    collision_comp.OnComponentBeginOverlap.AddDynamic(
        this, &UCollisionEffectSubsystem::handle_collision_event);

    return index;
}

void UCollisionEffectSubsystem::add_effect_to_collision(int32 i, UActorComponent& component) {
    auto* effect_interface{Cast<ICollisionEffectComponent>(&component)};
    if (!effect_interface) {
        return;
    }

    auto& effects{effect_components[i]};
    effects.Emplace(&component, effect_interface, effect_interface->get_execution_priority());
    effects.Sort();
}

bool UCollisionEffectSubsystem::is_valid_collision_entry(int32 index) const {
    return index >= 0 && index < collision_owners.Num() && collision_owners[index].IsValid();
}

UWorld* UCollisionEffectSubsystem::get_world_from_component(UActorComponent* component) {
    if (component) {
        if (auto* owner{component->GetOwner()}) {
            return owner->GetWorld();
        }
    }
    return nullptr;
}
