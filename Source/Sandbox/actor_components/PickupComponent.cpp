#include "Sandbox/actor_components/PickupComponent.h"

UPickupComponent::UPickupComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UPickupComponent::BeginPlay() {
    Super::BeginPlay();

    if (auto* pickup_owner{Cast<ICollisionOwner>(GetOwner())}) {
        collision_component = pickup_owner->get_collision_component();
        if (collision_component.IsValid()) {
            collision_component->OnComponentBeginOverlap.AddDynamic(this,
                                                                    &UPickupComponent::on_overlap);
        }
    }
}

void UPickupComponent::EndPlay(EEndPlayReason::Type const EndPlayReason) {
    if (collision_component.IsValid()) {
        collision_component->OnComponentBeginOverlap.RemoveDynamic(this,
                                                                   &UPickupComponent::on_overlap);
    }

    if (GetWorld() && cooldown_timer_handle.IsValid()) {
        GetWorld()->GetTimerManager().ClearTimer(cooldown_timer_handle);
    }

    Super::EndPlay(EndPlayReason);
}

void UPickupComponent::on_overlap(UPrimitiveComponent* OverlappedComponent,
                                  AActor* OtherActor,
                                  UPrimitiveComponent* OtherComp,
                                  int32 OtherBodyIndex,
                                  bool bFromSweep,
                                  FHitResult const& SweepResult) {
    log_verbose(TEXT("on_overlap."));

    if (is_on_cooldown || !OtherActor) {
        return;
    }

    if (auto* pickup_owner{Cast<ICollisionOwner>(GetOwner())}) {
        pickup_owner->on_pre_collision_effect(OtherActor);

        execute_pickup_effect(OtherActor);
        pickup_owner->on_collision_effect(OtherActor);

        pickup_owner->on_post_collision_effect(OtherActor);

        if (pickup_owner->should_destroy_after_collision()) {
            if (auto* destruction_manager{
                    GetWorld()->GetSubsystem<UDestructionManagerSubsystem>()}) {
                destruction_manager->queue_actor_destruction(GetOwner());
            }
        } else {
            auto const cooldown_time{pickup_owner->get_collision_cooldown()};
            if (cooldown_time > 0.0f) {
                is_on_cooldown = true;
                GetWorld()->GetTimerManager().SetTimer(cooldown_timer_handle,
                                                       this,
                                                       &UPickupComponent::reset_cooldown,
                                                       cooldown_time,
                                                       false);
            }
        }
    }
}

void UPickupComponent::reset_cooldown() {
    is_on_cooldown = false;
}
