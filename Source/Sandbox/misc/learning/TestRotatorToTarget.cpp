#include "TestRotatorToTarget.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>
#include <Materials/MaterialInstanceDynamic.h>
#include <Materials/MaterialInterface.h>

ATestRotatorToTarget::ATestRotatorToTarget()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ATestRotatorToTarget::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    configure_material();
}
void ATestRotatorToTarget::BeginPlay() {
    Super::BeginPlay();

    configure_material();
}
void ATestRotatorToTarget::Tick(float dt) {
    Super::Tick(dt);

    look_at_target(dt);
}

void ATestRotatorToTarget::configure_material() {
    material_config.initialise_mesh_material(this, *mesh, 0);
}
void ATestRotatorToTarget::look_at_target(float dt) {
    if (!target.IsValid()) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("Rotate target is invalid."));
        return;
    }

    auto const current_rotation{GetActorRotation()};
    auto const direction_to_target{
        (target->GetActorLocation() - GetActorLocation()).GetSafeNormal()};
    auto const rotation_to_target{direction_to_target.Rotation()};

    auto const new_rotation{FMath::RInterpConstantTo(
        current_rotation, rotation_to_target, dt, turning_speed_deg_per_s)};

    SetActorRotation(new_rotation);
}
