#include "Sandbox/subsystems/CollisionEffectSubsystem2.h"

#include "Sandbox/subsystems/DestructionManagerSubsystem.h"

void UCollisionEffectSubsystem2::handle_collision_event(UPrimitiveComponent* OverlappedComponent,
                                                        AActor* OtherActor,
                                                        UPrimitiveComponent* OtherComp,
                                                        int32 OtherBodyIndex,
                                                        bool bFromSweep,
                                                        FHitResult const& SweepResult) {
    if (!OtherActor || !OverlappedComponent) {
        log_warning(TEXT("No OtherActor or OverlappedComponent."));
        return;
    }

    auto const* index_ptr{collision_ids.Find(OverlappedComponent)};
    if (!index_ptr) {
        log_warning(TEXT("No collision entry."));
        return;
    }

    int32 const index{*index_ptr};
    auto* owner{actors[index].Get()};
    if (!owner) {
        log_warning(TEXT("No owner."));
        return;
    }

    // Should have already been validated
    auto& collision_owner{*Cast<ICollisionOwner>(owner)};

    // Execute the effects
    collision_owner.on_pre_collision_effect(OtherActor);
    // for (auto const& entry : effect_components[index]) {
    //     if (entry.component.IsValid() && entry.effect_interface) {
    //         entry.effect_interface->execute_effect(OtherActor);
    //     }
    // }
    collision_owner.on_collision_effect(OtherActor);
    collision_owner.on_post_collision_effect(OtherActor);

    if (collision_owner.should_destroy_after_collision()) {
        if (auto* destruction_manager{GetWorld()->GetSubsystem<UDestructionManagerSubsystem>()}) {
            destruction_manager->queue_actor_destruction(owner);
        }
    }
}
