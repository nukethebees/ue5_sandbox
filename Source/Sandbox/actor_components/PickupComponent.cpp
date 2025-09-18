#include "Sandbox/actor_components/PickupComponent.h"

UPickupComponent::UPickupComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UPickupComponent::BeginPlay() {
    Super::BeginPlay();

    if (auto* pickup_owner{Cast<IPickupOwner>(GetOwner())}) {
        collision_component = pickup_owner->get_pickup_collision_component();
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
    if (is_on_cooldown || !OtherActor) {
        return;
    }

    if (auto* pickup_owner{Cast<IPickupOwner>(GetOwner())}) {
        pickup_owner->on_pre_pickup_effect(OtherActor);

        execute_pickup_effect(OtherActor);

        pickup_owner->on_post_pickup_effect(OtherActor);

        if (pickup_owner->should_destroy_after_pickup()) {
            if (auto* destruction_manager{
                    GetWorld()->GetSubsystem<UDestructionManagerSubsystem>()}) {
                destruction_manager->queue_actor_destruction(GetOwner());
            }
        } else {
            auto const cooldown_time{pickup_owner->get_pickup_cooldown()};
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
