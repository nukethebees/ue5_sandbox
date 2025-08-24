#include "Sandbox/actor_components/InteractRotateComponent.h"

UInteractRotateComponent::UInteractRotateComponent() {
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UInteractRotateComponent::BeginPlay() {
    Super::BeginPlay();
}

void UInteractRotateComponent::TickComponent(float DeltaTime,
                                             ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    auto* const owner{GetOwner()};
    if (owner != nullptr) {
        auto const new_rotation{owner->GetActorRotation() + rotation_rate * DeltaTime};
        owner->SetActorRotation(new_rotation);
    }
}
void UInteractRotateComponent::interact(AActor* const interactor) {
    auto const* const owner{GetOwner()};
    auto* const world{GetWorld()};

    if (!owner || !world) {
        return;
    }
    PrimaryComponentTick.SetTickFunctionEnable(true);
    world->GetTimerManager().SetTimer(rotation_timer_handle,
                                      this,
                                      &UInteractRotateComponent::stop_rotation,
                                      rotation_duration_seconds,
                                      false);
}
void UInteractRotateComponent::stop_rotation() {
    PrimaryComponentTick.SetTickFunctionEnable(false);
}
