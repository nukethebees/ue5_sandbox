#include "TestTurrets.h"

#include <SandboxCore/Public/interpolation.h>
#include <SandboxCore/Public/rotation.h>

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ATestTurrets::ATestTurrets() {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bCanEverTick = true;
}

void ATestTurrets::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
}
void ATestTurrets::BeginPlay() {
    Super::BeginPlay();
}
void ATestTurrets::Tick(float dt) {
    Super::Tick(dt);

    integrate_rotations(dt);
    apply_rotations_to_components();
}

auto ATestTurrets::num_turrets() const noexcept -> int32 {
    return ai_states.Num();
}

void ATestTurrets::integrate_rotations(float dt) {
    auto const n{num_turrets()};

    ml::rotate_towards_1d_degrees_normalised_inplace(TArrayView<float>{pitch_degrees},
                                                     TConstArrayView<float>{target_pitch_degrees},
                                                     pitch_rotation_speed_degrees,
                                                     dt);

    ml::rotate_towards_1d_degrees_normalised_inplace(TArrayView<float>{yaw_degrees},
                                                     TConstArrayView<float>{target_yaw_degrees},
                                                     yaw_rotation_speed_degrees,
                                                     dt);
}
void ATestTurrets::apply_rotations_to_components() {
    auto const n{num_turrets()};

    for (int32 i{0}; i < n; ++i) {
        body_meshes[i]->SetRelativeRotation(FRotator{0.f, yaw_degrees[i], 0.f});
        barrel_meshes[i]->SetRelativeRotation(FRotator{pitch_degrees[i], 0.f, 0.f});
    }
}
