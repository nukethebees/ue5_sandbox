#include "Sandbox/subsystems/CollisionEffectSubsystem2.h"

#include "Sandbox/subsystems/DestructionManagerSubsystem.h"

void UCollisionEffectSubsystem2::handle_collision_event(UPrimitiveComponent* OverlappedComponent,
                                                        AActor* OtherActor,
                                                        UPrimitiveComponent* OtherComp,
                                                        int32 OtherBodyIndex,
                                                        bool bFromSweep,
                                                        FHitResult const& SweepResult) {
    log_verbose(TEXT("handle_collision_event"));

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

    auto& payload_indexes{actor_payload_indexes[index]};

    log_verbose(TEXT("Handling %d indexes"), payload_indexes.indexes.Num());

    for (auto const& payload_index : payload_indexes.indexes) {
        switch (payload_index.type_tag) {
            case 0: {
                std::get<0>(payloads)[payload_index.array_index].execute(OtherActor);
                break;
            }
            case 1: {
                std::get<1>(payloads)[payload_index.array_index].execute(OtherActor);
                break;
            }
            default: {
                log_warning(TEXT("Unhandled tag type: %d."), payload_index.type_tag);
                break;
            }
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
