#include "TestLasers.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestLasersConfig.h"

#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <Engine/HitResult.h>
#include <Engine/World.h>
#include <ProfilingDebugging/CountersTrace.h>

TRACE_DECLARE_INT_COUNTER(SandboxTestLaserCount, TEXT("Sandbox/TestLaserCount"));
TRACE_DECLARE_INT_COUNTER(SandboxTestLaserRemovedCount, TEXT("Sandbox/TestLaserRemovedCount"));

ATestLasers::ATestLasers()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

// Actor lifecycle
void ATestLasers::PostInitializeComponents() {
    Super::PostInitializeComponents();

    clear_runtime_state();
}
void ATestLasers::BeginPlay() {
    Super::BeginPlay();

    TRACE_COUNTER_SET(SandboxTestLaserCount, 0);
    TRACE_COUNTER_SET(SandboxTestLaserRemovedCount, 0);

    SetActorTickEnabled(true);

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

#if WITH_EDITOR
    debug_drawer.world = GetWorld();
#endif

    check(array_sizes_consistent());
}
void ATestLasers::Tick(float dt) {
    Super::Tick(dt);

    auto const n{get_num_instances()};
    if (n < 1) {
        return;
    }

    tick_lifetimes(dt);
    prune_old_instances();

    handle_collisions(dt);
    update_locations(dt);

    update_ismc();

    TRACE_COUNTER_SET(SandboxTestLaserCount, get_num_instances());

#if WITH_EDITOR
    dbg_n_instances = get_num_instances();
#endif
}

// Accessors
auto ATestLasers::get_num_instances() const noexcept -> int32 {
    return lifetimes.Num();
}

// Spawning / Configuration
void ATestLasers::spawn_lasers(TConstArrayView<FTransform> const new_transforms) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::spawn_lasers);

    auto const offset{get_num_instances()};
    auto const n_to_add{new_transforms.Num()};

    instances->AddInstances(TArray<FTransform>(new_transforms), false, true, false);

    transforms.Append(new_transforms);
    lifetimes.AddZeroed(n_to_add);
    velocities.AddUninitialized(n_to_add);

    auto const laser_speed{laser_config->speed};
    for (int32 i{0}; i < n_to_add; ++i) {
        auto const index{offset + i};

        velocities[index] = transforms[index].GetRotation().Vector() * laser_speed;
    }

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
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::update_locations);

    auto const n{get_num_instances()};

    for (int32 i{0}; i < n; ++i) {
        transforms[i].AddToTranslation(dt * velocities[i]);
    }
}
void ATestLasers::handle_collisions(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::handle_collisions);

    auto const n{get_num_instances()};

    if (n < 1) {
        return;
    }

    auto* world{GetWorld()};
    to_remove.Reset();

    for (int32 i{n - 1}; i >= 0; --i) {
        auto const start{transforms[i].GetLocation()};
        auto const end{start + dt * velocities[i]};

        FHitResult hit{};

        FCollisionQueryParams params{};
        params.AddIgnoredActor(this);

        auto const did_hit{
            world->LineTraceSingleByChannel(hit, start, end, ECC_Visibility, params)};

        if (did_hit) {
            to_remove.Add(i);

#if WITH_EDITOR
            if (debugging_shapes_enabled) {
                debug_drawer.draw_sphere(hit.ImpactPoint);
            }
#endif
        }
    }

    remove_instances(to_remove);
}

// Visuals
void ATestLasers::update_ismc() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::update_ismc);
    instances->BatchUpdateInstancesTransforms(0, transforms, true, true, false);
}

// Lifetimes
void ATestLasers::tick_lifetimes(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::tick_lifetimes);

    ml::add_in_place(TArrayView<float>{lifetimes}, dt);
    }
void ATestLasers::prune_old_instances() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::prune_old_instances);

    auto const n{get_num_instances()};
    if (n < 1) {
        return;
    }

    auto const laser_lifetime{laser_config->lifetime};

    to_remove.Reset();

    for (int32 i{n - 1}; i >= 0; --i) {
        if (lifetimes[i] >= laser_lifetime) {
            to_remove.Add(i);
        }
    }

    remove_instances(to_remove);
}

// Debugging
bool ATestLasers::array_sizes_consistent() const {
    auto const n{instances->GetNumInstances()};

    return ml::all_num_equal_to(n, transforms, velocities, lifetimes);
}

// Misc
void ATestLasers::clear_runtime_state() {
    instances->ClearInstances();

    transforms.Reset();
    velocities.Reset();
    lifetimes.Reset();
    to_remove.Reset();
}
void ATestLasers::remove_instances(TConstArrayView<int32> indices) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::remove_instances);

    auto const n{indices.Num()};
    if (n < 1) {
        return;
    }

    ml::remove_at_swap_many_sorted_desc(indices, transforms, velocities, lifetimes);

    TRACE_COUNTER_ADD(SandboxTestLaserRemovedCount, n);

    instances->RemoveInstances(to_remove, true);

    check(array_sizes_consistent());
}
