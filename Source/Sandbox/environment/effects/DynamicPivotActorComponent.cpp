#include "Sandbox/environment/effects/DynamicPivotActorComponent.h"

#include <ioj/presentation/observer_camera.h>
#include <ioj/rotator_math.h>
#include <ioj/vector_value_math.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCoreEngine/TransformConversion.h>

#include "Engine/HitResult.h"
#include "Engine/World.h"

UDynamicPivotActorComponent::UDynamicPivotActorComponent() {
    PrimaryComponentTick.bCanEverTick = true;
}
void UDynamicPivotActorComponent::BeginPlay() {
    Super::BeginPlay();

    auto const* const owner{GetOwner()};
    if (!owner) {
        UE_LOG(LogSandbox, Warning, TEXT("Dynamic pivot component has no owner."));
        return;
    }
    pivot_point = owner->GetActorLocation();
    auto const start{owner->GetActorLocation()};

    TArray<UActorComponent*> components{owner->GetComponents().Array()};
    auto const arrow_name{FName{"DynamicPivotArrow"}};
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
void UDynamicPivotActorComponent::TickComponent(float DeltaTime,
                                                ELevelTick TickType,
                                                FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    update_position(DeltaTime);
}

void UDynamicPivotActorComponent::update_position(float delta_time) {
    current_angle_deg += rotation_speed_deg_per_sec * delta_time;
    auto* const owner{GetOwner()};
    if (!owner) {
        UE_LOG(LogSandbox, Warning, TEXT("Dynamic pivot component has no owner."));
        return;
    }
    auto const rotation{ioj::to_quaternion(ioj::Rotator3d{0.0, current_angle_deg, 0.0})};
    auto const offset{ioj::rotate_vector(rotation, ioj::Vector3d{initial_offset})};
    auto const pivot{ioj::Vector3d{pivot_point}};
    auto const camera{ioj::presentation::make_observer_camera_transform(
        std::span{&pivot, 1}, offset, ioj::size(offset))};
    if (!camera) {
        UE_LOG(LogSandbox, Warning, TEXT("Dynamic pivot requires a finite, nonzero offset."));
        SetComponentTickEnabled(false);
        return;
    }
    auto const transform{ioj::to_unreal(camera->transform)};
    owner->SetActorLocationAndRotation(transform.GetLocation(), transform.GetRotation());
}
