#include "Sandbox/actor_components/LoopingLiftComponent.h"

ULoopingLiftComponent::ULoopingLiftComponent() {
    PrimaryComponentTick.bCanEverTick = true;
}

void ULoopingLiftComponent::BeginPlay() {
    Super::BeginPlay();
    origin = GetOwner()->GetActorLocation();
}

void ULoopingLiftComponent::TickComponent(float DeltaTime,
                                          ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (pause_timer <= pause_after_completion) {
        pause_timer += DeltaTime;
        return;
    }

    auto const up{GetOwner()->GetActorLocation()};
    auto const dist_from_origin{up.Z - origin.Z};

    if (going_up) {
        if (dist_from_origin < distance) {
            GetOwner()->AddActorWorldOffset(FVector::UpVector * speed * DeltaTime);
        } else {
            going_up = false;
            pause_timer = 0.0f;
        }
    } else {
        if (dist_from_origin > 0) {
            GetOwner()->AddActorWorldOffset(FVector::DownVector * speed * DeltaTime);
        } else {
            going_up = true;
            pause_timer = 0.0f;
        }
    }
}
