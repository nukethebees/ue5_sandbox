#include "TestLasers.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestLasersConfig.h"

#include <SandboxCore/Public/array_utils.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>

ATestLasers::ATestLasers()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

// Actor lifecycle
void ATestLasers::BeginPlay() {
    Super::BeginPlay();

    if (!laser_config) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("laser_config is nullptr"));
        SetActorTickEnabled(false);
        return;
    }

    if (!laser_config->is_ready()) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("laser_config is not ready."));
        SetActorTickEnabled(false);
        return;
    }

    configure_ismc();

    check(array_sizes_consistent());
}
void ATestLasers::Tick(float dt) {
    Super::Tick(dt);

    tick_lifetimes(dt);
    prune_old_instances();

    handle_collisions(dt);
    update_locations(dt);

    update_ismc();
}

// Accessors
auto ATestLasers::get_num_instances() const noexcept -> int32 {
    return lifetimes.Num();
}

// Spawning / Configuration
void ATestLasers::spawn_laser(FTransform const& transform, AActor const& owner_to_ignore) {
    // Later on this can just add the data to a queue and then batch spawn them laser
    instances->AddInstance(transform, true);
    velocities.Add(transform.GetRotation().Vector().GetSafeNormal() * laser_config->speed);
    transforms.Add(transform);
    lifetimes.Add(0.f);

    check(array_sizes_consistent());
}

void ATestLasers::configure_ismc() {
    instances->SetStaticMesh(laser_config->mesh);
    instances->SetMaterial(0, laser_config->material);

    instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    instances->SetGenerateOverlapEvents(false);

    instances->SetCanEverAffectNavigation(false);

    instances->SetMobility(EComponentMobility::Movable);
}

// Movement
void ATestLasers::update_locations(float const dt) {
    auto const n{get_num_instances()};

    for (int32 i{0}; i < n; ++i) {
        transforms[i].AddToTranslation(dt * velocities[i]);
    }
}
void ATestLasers::handle_collisions(float const dt) {}

// Visuals
void ATestLasers::update_ismc() {
    instances->BatchUpdateInstancesTransforms(0, transforms, true, true, false);
}

// Lifetimes
void ATestLasers::tick_lifetimes(float const dt) {
    auto const n{get_num_instances()};

    for (int32 i{0}; i < n; ++i) {
        lifetimes[i] -= dt;
    }
}
void ATestLasers::prune_old_instances() {
    auto const n{get_num_instances()};
    auto const threshold{laser_config->lifetime};

    to_remove.Reset();

    for (int32 i{n - 1}; i >= 0; --i) {
        if (lifetimes[i] >= threshold) {
            transforms.RemoveAtSwap(i, EAllowShrinking::No);
            velocities.RemoveAtSwap(i, EAllowShrinking::No);
            lifetimes.RemoveAtSwap(i, EAllowShrinking::No);

            to_remove.Add(i);
        }
    }

    instances->RemoveInstances(to_remove);

    check(array_sizes_consistent());
}

// Debugging
bool ATestLasers::array_sizes_consistent() const {
    auto const n{instances->GetNumInstances()};

    return ml::all_num_equal_to(n, transforms, velocities, lifetimes);
}
