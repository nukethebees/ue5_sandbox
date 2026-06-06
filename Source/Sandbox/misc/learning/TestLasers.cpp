#include "TestLasers.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestLasersConfig.h"

#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/invoke.h>
#include <SandboxCore/uobject_utils.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <Engine/HitResult.h>
#include <Engine/World.h>
#include <ProfilingDebugging/CountersTrace.h>

TRACE_DECLARE_INT_COUNTER(SandboxTestLaserCount, TEXT("Sandbox/TestLaserCount"));
TRACE_DECLARE_INT_COUNTER(SandboxTestLaserISMCCount, TEXT("Sandbox/TestLaserISMCCount"));
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

    // An identity transform means instance world and relative transforms are identical
    SetActorTransform(FTransform::Identity, false);

    TRACE_COUNTER_SET(SandboxTestLaserCount, 0);
    TRACE_COUNTER_SET(SandboxTestLaserRemovedCount, 0);

    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(laser_config),
    });
    if (!laser_config->is_ready()) {
        UE_LOG(LogSandboxLearning, Fatal, TEXT("laser_config is not ready."));
    }

    preallocate_instances();
    configure_ismc();

#if WITH_EDITOR
    debug_drawer.world = GetWorld();
#endif

    check(array_sizes_consistent());
}
void ATestLasers::Tick(float dt) {
    Super::Tick(dt);

    tick(dt);
}
void ATestLasers::tick(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::tick);

    tick_lifetimes(dt);
    collect_old_instance_indices();
    remove_instances(to_remove);

    handle_collisions(dt);
    update_locations(dt);

    process_pending_spawns();
    update_ismc();

    TRACE_COUNTER_SET(SandboxTestLaserCount, get_num_instances());
    TRACE_COUNTER_SET(SandboxTestLaserISMCCount, instances->GetNumInstances());

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

    transforms_to_add.Append(new_transforms);
}
void ATestLasers::preallocate_instances() {
    instances->PreAllocateInstancesMemory(n_preallocated_instances);
    ml::invoke_on_all([n = this->n_preallocated_instances](auto& array) { array.Reserve(n); },
                      transforms,
                      velocities,
                      lifetimes);
}
void ATestLasers::process_pending_spawns() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::process_pending_spawns);

    auto const offset{get_num_instances()};
    auto const n_to_add{transforms_to_add.Num()};

    auto const cur_total{get_num_instances()};
    auto const new_total{cur_total + n_to_add};

    auto const n_ismc_instances{instances->GetNumInstances()};
    auto const n_ismc_instances_to_add{new_total - n_ismc_instances};
    if (n_ismc_instances_to_add > 0) {
        TArray<FTransform> dummy_transforms;
        dummy_transforms.AddDefaulted(n_ismc_instances_to_add);
        instances->AddInstances(dummy_transforms, false, is_world_space, false);
    }

    transforms.Append(transforms_to_add);
    lifetimes.AddZeroed(n_to_add);
    velocities.AddUninitialized(n_to_add);

    auto const laser_speed{laser_config->speed};
    for (int32 i{0}; i < n_to_add; ++i) {
        auto const index{offset + i};

        velocities[index] = transforms[index].GetRotation().Vector() * laser_speed;
    }

    check(array_sizes_consistent());

    transforms_to_add.Reset();
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

    FHitResult hit{};

    FCollisionQueryParams params{};
    params.AddIgnoredActor(this);

    for (int32 i{n - 1}; i >= 0; --i) {
        auto const start{transforms[i].GetLocation()};
        auto const end{start + dt * velocities[i]};

        auto const did_hit{
            world->LineTraceSingleByChannel(hit, start, end, ECC_Visibility, params)};

        if (did_hit) {
            to_remove.Add(i);
        }
    }

    remove_instances(to_remove);
}

// Visuals
void ATestLasers::configure_ismc() {
    instances->SetStaticMesh(laser_config->mesh);
    instances->SetMaterial(0, laser_config->material);

    instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    instances->SetGenerateOverlapEvents(false);

    instances->SetCanEverAffectNavigation(false);

    instances->SetMobility(EComponentMobility::Movable);

    instances->SetCastShadow(false);
    instances->SetAffectDistanceFieldLighting(false);
    instances->SetReceivesDecals(false);

    instances->SetCullDistances(laser_config->min_cull_distance, laser_config->max_cull_distance);
}
void ATestLasers::update_ismc() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::update_ismc);
    instances->BatchUpdateInstancesTransforms(0, transforms, is_world_space, false, false);

    auto const n_instances{get_num_instances()};
    auto const n_ismcs{instances->GetNumInstances()};
    auto const n_to_hide{n_ismcs - n_instances};
    if (n_to_hide > 0) {
        auto hidden_transform{FTransform::Identity};
        hidden_transform.SetScale3D(FVector{0.0});
        TArray<FTransform> hidden_transforms;
        hidden_transforms.Reserve(n_to_hide);
        for (int32 i{0}; i < n_to_hide; ++i) {
            hidden_transforms.Add(hidden_transform);
        }
        instances->BatchUpdateInstancesTransforms(
            n_instances, hidden_transforms, is_world_space, false, false);
    }

    instances->MarkRenderStateDirty();
}

// Lifetimes
void ATestLasers::tick_lifetimes(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::tick_lifetimes);

    ml::add_in_place(TArrayView<float>{lifetimes}, dt);
}
void ATestLasers::collect_old_instance_indices() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::collect_old_instance_indices);

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
}

// Debugging
bool ATestLasers::array_sizes_consistent() const {
    auto const consistent{ml::all_num_equal(transforms, velocities, lifetimes)};

    return consistent && (instances->GetNumInstances() >= get_num_instances());
}

// Misc
void ATestLasers::clear_runtime_state() {
    instances->ClearInstances();
    ml::reset_arrays(transforms, velocities, lifetimes, transforms_to_add, to_remove);
}
void ATestLasers::remove_instances(TConstArrayView<int32> indices) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::remove_instances);

    auto const n{indices.Num()};
    if (n < 1) {
        return;
    }

    {
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::remove_instances::remove_at_swap);
        ml::remove_at_swap_many_sorted_desc(indices, transforms, velocities, lifetimes);
    }

    check(array_sizes_consistent());
    TRACE_COUNTER_SET(SandboxTestLaserRemovedCount, n);
}
