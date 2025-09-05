#include "Sandbox/actor_components/RotatingCameraComponent.h"

#include "Camera/CameraActor.h"

URotatingCameraComponent::URotatingCameraComponent() {
    PrimaryComponentTick.bCanEverTick = true;
}
void URotatingCameraComponent::BeginPlay() {
    Super::BeginPlay();

    auto const* const owner{GetOwner()};
    if (owner) {
        pivot_point = owner->GetActorLocation();
    }
}
void URotatingCameraComponent::TickComponent(float DeltaTime,
                                             ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    update_camera_position(DeltaTime);
}

void URotatingCameraComponent::update_camera_position(float delta_time) {
    current_angle_deg += rotation_speed_deg_per_sec * delta_time;

    auto const radians{FMath::DegreesToRadians(current_angle_deg)};
    FVector const offset{radius * FMath::Cos(radians), radius * FMath::Sin(radians), height};

    auto* const owner{GetOwner()};
    if (owner) {
        auto const new_location{pivot_point + offset};
        auto const look_rotation{(pivot_point - owner->GetActorLocation()).Rotation()};

        owner->SetActorLocation(new_location);
        owner->SetActorRotation(look_rotation);
    }
}
