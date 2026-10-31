#include "Sandbox/core/camera/StaticOrbitCameraComponent.h"

#include <ioj/presentation/observer_camera.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCoreEngine/TransformConversion.h>

#include <GameFramework/Actor.h>

UStaticOrbitCameraComponent::UStaticOrbitCameraComponent() {
    PrimaryComponentTick.bCanEverTick = true;
}
void UStaticOrbitCameraComponent::BeginPlay() {
    Super::BeginPlay();

    auto const* const owner{GetOwner()};
    if (owner) {
        pivot_point_ = ioj::Vector3d{owner->GetActorLocation()};
    }
}
void UStaticOrbitCameraComponent::TickComponent(float DeltaTime,
                                                ELevelTick TickType,
                                                FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    update_camera_position(DeltaTime);
}

void UStaticOrbitCameraComponent::update_camera_position(float delta_time) {
    current_angle_deg_ += rotation_speed_deg_per_sec * delta_time;
    auto const camera{ioj::presentation::orbit_camera_transform(
        pivot_point_, current_angle_deg_, radius, height)};
    if (!camera) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("Orbit camera requires finite inputs and a positive radius."));
        SetComponentTickEnabled(false);
        return;
    }

    auto* const owner{GetOwner()};
    if (!owner) {
        UE_LOG(LogSandbox, Warning, TEXT("Orbit camera has no owner."));
        return;
    }
    auto const transform{ioj::to_unreal(*camera)};
    owner->SetActorLocationAndRotation(transform.GetLocation(), transform.GetRotation());
}
