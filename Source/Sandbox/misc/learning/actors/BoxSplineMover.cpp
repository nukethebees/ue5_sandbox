#include "Sandbox/actors/BoxSplineMover.h"

#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"

ABoxSplineMover::ABoxSplineMover() {
    PrimaryActorTick.bCanEverTick = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Movable);

    spline_component = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
    spline_component->SetupAttachment(RootComponent);

    mesh_component = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    mesh_component->SetupAttachment(RootComponent);
}

void ABoxSplineMover::BeginPlay() {
    Super::BeginPlay();

    spline_length = spline_component->GetSplineLength();
    current_distance = 0.0f;
    moving_forward = true;

    if (spline_length > 0.0f) {
        update_position_along_spline(current_distance);
    }
}

void ABoxSplineMover::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    if (spline_length <= 0.0f) {
        return;
    }

    auto const distance_delta{movement_speed * DeltaTime};

    if (loop_movement) {
        current_distance += distance_delta;

        if (current_distance >= spline_length) {
            current_distance = FMath::Fmod(current_distance, spline_length);
        }
    } else {
        if (moving_forward) {
            current_distance += distance_delta;
            if (current_distance >= spline_length) {
                current_distance = spline_length;
                moving_forward = false;
            }
        } else {
            current_distance -= distance_delta;
            if (current_distance <= 0.0f) {
                current_distance = 0.0f;
                moving_forward = true;
            }
        }
    }

    update_position_along_spline(current_distance);
}

void ABoxSplineMover::update_position_along_spline(float distance) {
    constexpr auto space{ESplineCoordinateSpace::World};

    auto const location{spline_component->GetLocationAtDistanceAlongSpline(distance, space)};
    auto const rotation{spline_component->GetRotationAtDistanceAlongSpline(distance, space)};

    mesh_component->SetWorldLocationAndRotation(location, rotation);
}
