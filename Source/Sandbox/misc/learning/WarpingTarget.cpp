#include "WarpingTarget.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/misc/learning/WarpVolume.h"

#include <Components/BoxComponent.h>
#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>
#include <Kismet/KismetMathLibrary.h>
#include <Materials/MaterialInstanceDynamic.h>
#include <Materials/MaterialInterface.h>

AWarpingTarget::AWarpingTarget()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void AWarpingTarget::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    initialise_material();
}
void AWarpingTarget::BeginPlay() {
    Super::BeginPlay();

    SetActorTickEnabled(warp_periodically);
    initialise_material();

    if (!assert_valid_volume()) { SetActorTickEnabled(false); }

    if (warp_on_first_tick) {
        warp_cooldown.finish();
    } else {
        warp_cooldown.reset();
    }
}
void AWarpingTarget::Tick(float dt) {
    Super::Tick(dt);

    warp_cooldown.tick(dt);

    if (warp_cooldown.reset_if_done()) { warp(); }

    if ((destroy_after >= 0.f) && (GetGameTimeSinceCreation() > destroy_after)) { Destroy(); }
}

auto AWarpingTarget::warp() -> bool {
    if (!assert_valid_volume()) { return false; }

    auto* box{warp_volume->get_box()};
    auto const point{UKismetMathLibrary::RandomPointInBoundingBox(box->GetComponentLocation(),
                                                                  box->GetScaledBoxExtent())};

    SetActorLocation(point);

    return true;
}
void AWarpingTarget::warp_now() {
    if (!warp()) { UE_LOG(LogSandboxLearning, Warning, TEXT("warp failed.")); }
}
auto AWarpingTarget::assert_valid_volume() const -> bool {
    if (!warp_volume) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("warp_volume is nullptr"));
        return false;
    }
    return true;
}
void AWarpingTarget::initialise_material() {
    material_config.initialise_mesh_material(this, *mesh, 0);
}
