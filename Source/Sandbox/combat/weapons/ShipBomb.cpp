#include "Sandbox/combat/weapons/ShipBomb.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

AShipBomb::AShipBomb()
    : mesh_component{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh_component->SetupAttachment(RootComponent);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AShipBomb::Tick(float dt) {
    Super::Tick(dt);

    auto const velocity{GetActorForwardVector() * speed};
    auto const delta_pos{velocity * dt};
    SetActorLocation(GetActorLocation() + delta_pos);
}

void AShipBomb::detonate() {
    UE_LOG(LogSandboxActor, Verbose, TEXT("Bomb detonating."));

    auto& tm{GetWorldTimerManager()};
    tm.ClearTimer(timer);

    TRY_INIT_PTR(world, GetWorld());
    auto const location{GetActorLocation()};

    constexpr bool persistent_lines{false};
    constexpr int32 segments{16};
    constexpr auto lifetime{5.0f};
    constexpr uint8 depth_priority{0};
    constexpr auto thickness{15.0f};
    DrawDebugSphere(world,
                    location,
                    explosion_radius,
                    segments,
                    FColor::Orange,
                    persistent_lines,
                    lifetime,
                    depth_priority,
                    thickness);

    Destroy();
}

void AShipBomb::BeginPlay() {
    Super::BeginPlay();

    auto& tm{GetWorldTimerManager()};
    tm.SetTimer(timer, this, &ThisClass::detonate, fuse_time, false);
}
