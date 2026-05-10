#include "TestFlyOrbit3D.h"

#include <Components/ArrowComponent.h>
#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>

ATestFlyOrbit3D::ATestFlyOrbit3D()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))}
    , fwd_arrow{CreateDefaultSubobject<UArrowComponent>(TEXT("ForwardArrow"))}
    , right_arrow{CreateDefaultSubobject<UArrowComponent>(TEXT("RightArrow"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);

    fwd_arrow->SetupAttachment(RootComponent);
    fwd_arrow->SetArrowColor(FLinearColor::Red);
    fwd_arrow->SetRelativeRotation(FRotator::ZeroRotator);

    right_arrow->SetupAttachment(RootComponent);
    right_arrow->SetArrowColor(FLinearColor::Green);
    right_arrow->SetRelativeRotation(FRotator{0.0, 90.0, 0.0});

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ATestFlyOrbit3D::Tick(float dt) {
    Super::Tick(dt);

    angle_rad += FMath::DegreesToRadians(rotation_speed_deg_per_s) * dt;
    angle_rad = FMath::UnwindRadians(angle_rad);

    auto const x{radius * FMath::Cos(angle_rad)};
    auto const y{radius * FMath::Sin(angle_rad)};

    FVector const delta_pos{x * u + y * v};

    SetActorLocation(origin + delta_pos);
}

void ATestFlyOrbit3D::BeginPlay() {
    Super::BeginPlay();

    origin = GetActorLocation();
    u = GetActorForwardVector();
    v = GetActorRightVector();
}
