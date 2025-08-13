#include "Sandbox/actor_components/RotatingActorComponent.h"

URotatingActorComponent::URotatingActorComponent() {
    PrimaryComponentTick.bCanEverTick = true;
}

void URotatingActorComponent::BeginPlay() {
    Super::BeginPlay();
}

void URotatingActorComponent::TickComponent(float DeltaTime,
                                            ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    auto const delta_rotation{FRotator(0.0f, speed * DeltaTime, 0.0f)};
    GetOwner()->AddActorLocalRotation(delta_rotation);
}
