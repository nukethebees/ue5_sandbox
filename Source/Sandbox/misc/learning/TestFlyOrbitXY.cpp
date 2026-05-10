#include "TestFlyOrbitXY.h"

#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>

ATestFlyOrbitXY::ATestFlyOrbitXY()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ATestFlyOrbitXY::Tick(float dt) {
    Super::Tick(dt);

    angle_rad += FMath::DegreesToRadians(rotation_speed_deg_per_s) * dt;
    angle_rad = FMath::UnwindRadians(angle_rad);

    auto const x{radius * FMath::Cos(angle_rad)};
    auto const y{radius * FMath::Sin(angle_rad)};

    FVector const delta_pos{x, y, 0.f};

    SetActorLocation(origin + delta_pos);
}

void ATestFlyOrbitXY::BeginPlay() {
    Super::BeginPlay();

    origin = GetActorLocation();
}
