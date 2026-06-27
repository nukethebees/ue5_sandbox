#include "TestLasers.h"

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestLasersConfig.h>
#include <Sandbox/batch_game/TestStaticTurrets.h>
#include <Sandbox/batch_game/TestTubeSpinners.h>
#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/utilities/actor_utils.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/generation_index.h>
#include <SandboxCore/invoke.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/transforms.h>
#include <SandboxCore/uobject_utils.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <Engine/HitResult.h>
#include <Engine/World.h>
#include <ProfilingDebugging/CountersTrace.h>

TRACE_DECLARE_INT_COUNTER(SandboxTestLaserCount, TEXT("Sandbox/TestLaserCount"));
TRACE_DECLARE_INT_COUNTER(SandboxTestLaserISMCCount, TEXT("Sandbox/TestLaserISMCCount"));

void FTestLasersSpawnRequests::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(locations),
        SANDBOX_NAMED_NUM(rotations),
        SANDBOX_NAMED_NUM(damages),
        SANDBOX_NAMED_NUM(speeds),
        SANDBOX_NAMED_NUM(max_distances),
        SANDBOX_NAMED_NUM(instigator_handles),
    });
}
void FTestLasersSpawnRequests::reset() {
    ml::reset(locations, rotations, damages, speeds, max_distances, instigator_handles);
}
auto FTestLasersSpawnRequests::num() const noexcept -> int32 {
    return locations.num();
}
void FTestLasersSpawnRequests::reserve(int32 const count) {
    ml::reserve(count, locations, rotations, damages, speeds, max_distances, instigator_handles);
}
void FTestLasersSpawnRequests::add_uninitialised(int32 const count) {
    ml::add_uninitialised(
        count, locations, rotations, damages, speeds, max_distances, instigator_handles);
}

void FTestLasersSpawnRequests::set_damages(int32 const value) {
    ml::fill(damages, value);
}
void FTestLasersSpawnRequests::set_speeds(float const value) {
    ml::fill(speeds, value);
}
void FTestLasersSpawnRequests::set_max_distances(float const value) {
    ml::fill(max_distances, value);
}

void FTestLasersSpawnRequests::append_from(FTestLasersSpawnRequests const& other) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestLasersSpawnRequests::append_from);

    other.validate_array_sizes();

    ml::append_from(locations, other.locations);
    ml::append_from(rotations, other.rotations);
    damages.Append(other.damages);
    speeds.Append(other.speeds);
    max_distances.Append(other.max_distances);
    instigator_handles.Append(other.instigator_handles);
}

ATestLasers::ATestLasers()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    ml::set_actor_component_mobility(*this, EComponentMobility::Static);
}

// Actor lifecycle
void ATestLasers::begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::begin_play);
    TRACE_COUNTER_SET(SandboxTestLaserCount, 0);
    TRACE_COUNTER_SET(SandboxTestLaserISMCCount, 0);

    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(actor_config),
        SANDBOX_NAMED_UOBJECT_PTR(entity_registry),
    });
    if (!actor_config->is_ready()) {
        UE_LOG(LogSandboxLearning, Fatal, TEXT("actor_config is not ready."));
    }

    preallocate_instances();
    configure_ismc();

#if WITH_EDITOR
    debug_drawer.world = GetWorld();
#endif

    validate_array_sizes();
}

void ATestLasers::begin_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::begin_tick);
    clear_hit_buffers();
}
void ATestLasers::commit_spawns() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::commit_spawns);
    process_pending_spawns();
    clear_spawn_buffers();
}
void ATestLasers::simulate(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::tick);

    tick_lifetimes(dt);
    collect_old_instance_indices();
    remove_instances(to_remove);

    handle_collisions(dt);
    update_locations(dt);
}
void ATestLasers::update_visuals() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::update_visuals);

    update_ismc();
}
void ATestLasers::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::end_tick);
    TRACE_COUNTER_SET(SandboxTestLaserCount, get_num_instances());
    TRACE_COUNTER_SET(SandboxTestLaserISMCCount, instances->GetNumInstances());

    validate_array_sizes();
}

// Accessors
auto ATestLasers::get_num_instances() const noexcept -> int32 {
    return lifetimes_remaining.Num();
}

// Spawning / Configuration
void ATestLasers::spawn_lasers(FTestLasersSpawnRequests const& spawn_data) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::spawn_lasers);

    pending_spawns.append_from(spawn_data);
}
void ATestLasers::preallocate_instances() {
    instances->PreAllocateInstancesMemory(n_preallocated_instances);
    ml::invoke_on_all([n = this->n_preallocated_instances](auto& array) { ml::reserve(array, n); },
                      locations,
                      rotations,
                      velocities,
                      lifetimes_remaining);
}
void ATestLasers::process_pending_spawns() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::process_pending_spawns);

    pending_spawns.validate_array_sizes();
    auto const n_to_add{ml::num(pending_spawns)};
    if (n_to_add <= 0) { return; }

    auto const offset{get_num_instances()};

    auto const cur_total{get_num_instances()};
    auto const new_total{cur_total + n_to_add};

    auto const n_ismc_instances{instances->GetNumInstances()};
    auto const n_ismc_instances_to_add{new_total - n_ismc_instances};
    if (n_ismc_instances_to_add > 0) {
        TArray<FTransform> dummy_transforms;
        dummy_transforms.AddDefaulted(n_ismc_instances_to_add);
        instances->AddInstances(dummy_transforms, false, is_world_space, false);
    }

    ml::append_from(locations, pending_spawns.locations);
    ml::append_from(rotations, pending_spawns.rotations);
    damages.Append(pending_spawns.damages);
    instigator_handles.Append(pending_spawns.instigator_handles);

    ml::add_uninitialised(n_to_add, velocities, lifetimes_remaining);

    for (int32 i{0}; i < n_to_add; ++i) {
        auto const speed{pending_spawns.speeds[i]};
        auto const max_distance{pending_spawns.max_distances[i]};
        auto const lifetime{max_distance / speed};

        auto const index{offset + i};

        auto const velocity{ml::get_rotator3f(rotations, index).Vector() * speed};

        ml::assign(velocities, index, velocity);
        lifetimes_remaining[index] = lifetime;
    }

    validate_array_sizes();
    clear_spawn_buffers();
}

// Movement
void ATestLasers::update_locations(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::update_locations);
    ml::add_scaled_in_place(locations, velocities, dt);
}
void ATestLasers::handle_collisions(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::handle_collisions);

    auto const n{get_num_instances()};

    if (n < 1) { return; }

    auto* world{GetWorld()};

    FHitResult hit{};

    FCollisionQueryParams params{};
    params.AddIgnoredActor(this);

    ml::reset(to_remove, damage_events);

    for (int32 i{n - 1}; i >= 0; --i) {
        auto const start{ml::get_vector3d(locations, i)};
        auto const end{start + dt * FVector{velocities.xs[i], velocities.ys[i], velocities.zs[i]}};

        auto const did_hit{
            world->LineTraceSingleByChannel(hit, start, end, ECC_Visibility, params)};

        if (!did_hit) { continue; }
        to_remove.Add(i);

        // Identify who we hit
        auto* hit_actor{hit.GetActor()};
        if (!IsValid(hit_actor)) { continue; }

        auto* hit_component{hit.GetComponent()};
        if (!IsValid(hit_component)) { continue; }

        damage_events.damage_amounts.Add(damages[i]);
        damage_events.damaged_actors.Add(hit_actor);
        damage_events.actor_components.Add(hit_component);
        damage_events.hit_items.Add(hit.Item);
        damage_events.instigators.Add(instigator_handles[i]);
    }

    entity_registry->queue_damage_events(damage_events);

    remove_instances(to_remove);
}

// Visuals
void ATestLasers::configure_ismc() {
    instances->SetStaticMesh(actor_config->mesh);
    instances->SetMaterial(0, actor_config->material);

    instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    instances->SetGenerateOverlapEvents(false);

    instances->SetCanEverAffectNavigation(false);

    instances->SetCastShadow(false);
    instances->SetAffectDistanceFieldLighting(false);
    instances->SetReceivesDecals(false);

    instances->SetCullDistances(actor_config->min_cull_distance, actor_config->max_cull_distance);
}
void ATestLasers::update_ismc() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::update_ismc);

    update_ismc_transforms();
    instances->BatchUpdateInstancesTransforms(0, ismc_transforms, is_world_space, false, false);

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
void ATestLasers::update_ismc_transforms() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::update_ismc_transforms);

    auto const n{get_num_instances()};
    ismc_transforms.Reset();

    auto const n_transforms{ismc_transforms.Num()};
    auto const n_to_add(n - n_transforms);
    if (n_to_add > 0) { ismc_transforms.AddDefaulted(n_to_add); }

    for (int32 i{0}; i < n; ++i) {
        ismc_transforms[i].SetLocation(ml::get_vector3d(locations, i));

        auto const rotation{ml::get_rotator3d(rotations, i)};
        ismc_transforms[i].SetRotation(rotation.Quaternion());
    }
}

// Lifetimes
void ATestLasers::tick_lifetimes(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::tick_lifetimes);

    ml::subtract_in_place(TArrayView<float>{lifetimes_remaining}, dt);
}
void ATestLasers::collect_old_instance_indices() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::collect_old_instance_indices);

    auto const n{get_num_instances()};
    if (n < 1) { return; }

    to_remove.Reset();

    for (int32 i{n - 1}; i >= 0; --i) {
        if (lifetimes_remaining[i] <= 0.f) { to_remove.Add(i); }
    }
}

// Misc
void ATestLasers::clear_runtime_state() {
    instances->ClearInstances();
    ml::reset(ismc_transforms,
              locations,
              rotations,
              velocities,
              lifetimes_remaining,
              instigator_handles,
              damage_events);
    clear_spawn_buffers();
}
void ATestLasers::remove_instances(TConstArrayView<int32> indices) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::remove_instances);

    auto const n{indices.Num()};
    if (n < 1) { return; }

    {
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::remove_instances::remove_at_swap);
        ml::remove_at_swap_many_sorted_desc(indices,
                                            locations,
                                            rotations,
                                            velocities,
                                            damages,
                                            lifetimes_remaining,
                                            instigator_handles);
    }

    validate_array_sizes();
}
void ATestLasers::clear_spawn_buffers() {
    ml::reset(pending_spawns, to_remove);
}
void ATestLasers::clear_hit_buffers() {
    ml::reset(damage_events);
}

// Checks
void ATestLasers::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(locations),
        SANDBOX_NAMED_NUM(rotations),
        SANDBOX_NAMED_NUM(velocities),
        SANDBOX_NAMED_NUM(damages),
        SANDBOX_NAMED_NUM(lifetimes_remaining),
        SANDBOX_NAMED_NUM(instigator_handles),
    });

    auto const n{get_num_instances()};
    auto const n_ismc{instances->GetNumInstances()};
    if (n_ismc < n) {
        UE_LOG(LogSandbox,
               Fatal,
               TEXT("ATestLasers::validate_array_sizes %d entities, %d ISMC instances"),
               n,
               n_ismc);
    }
}
