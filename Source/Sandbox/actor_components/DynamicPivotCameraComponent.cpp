#include "Sandbox/actor_components/DynamicPivotCameraComponent.h"

#include "Camera/CameraActor.h"

UDynamicPivotCameraComponent::UDynamicPivotCameraComponent() {
    PrimaryComponentTick.bCanEverTick = true;
}
void UDynamicPivotCameraComponent::BeginPlay() {
    Super::BeginPlay();

    auto const* const owner{GetOwner()};
    if (owner) {
        pivot_point = owner->GetActorLocation();
    }

    // Get arrow used for positioning
    // If it doesn't exist then do a raycast instead
    auto const start{owner->GetActorLocation()};

    TArray<UActorComponent*> components{owner->GetComponents().Array()};
    auto const arrow_name{FName{"LookArrow"}};
    for (auto* component : components) {
        if (component->ComponentHasTag(arrow_name)) {
            if (auto const* const scene_component{Cast<USceneComponent>(component)}) {
                pivot_point = scene_component->GetComponentLocation();
                initial_offset = start - pivot_point;
                return;
            }
        }
    }

    auto const direction{owner->GetActorForwardVector()};
    auto const end{start + direction * 10000.0f};

    FHitResult hit_result{};
    FCollisionQueryParams query_params{};
    query_params.AddIgnoredActor(owner);

    bool const hit{
        GetWorld()->LineTraceSingleByChannel(hit_result, start, end, ECC_Visibility, query_params)};
    pivot_point = hit ? hit_result.ImpactPoint : FVector{0.0f, 0.0f, 0.0f};

    initial_offset = start - pivot_point;
}
void UDynamicPivotCameraComponent::TickComponent(float DeltaTime,
                                                 ELevelTick TickType,
                                                 FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    update_camera_position(DeltaTime);
}

void UDynamicPivotCameraComponent::update_camera_position(float delta_time) {
    current_angle_deg += rotation_speed_deg_per_sec * delta_time;
    current_angle_deg += rotation_speed_deg_per_sec * delta_time;

    float const radians{FMath::DegreesToRadians(current_angle_deg)};
    FVector const rotated_offset{
        initial_offset.X * FMath::Cos(radians) - initial_offset.Y * FMath::Sin(radians),
        initial_offset.X * FMath::Sin(radians) + initial_offset.Y * FMath::Cos(radians),
        initial_offset.Z // preserve height
    };

    auto* const owner{GetOwner()};
    if (owner) {
        FVector const new_location{pivot_point + rotated_offset};
        FRotator const look_rotation{(pivot_point - new_location).Rotation()};

        owner->SetActorLocation(new_location);
        owner->SetActorRotation(look_rotation);
    }
}
