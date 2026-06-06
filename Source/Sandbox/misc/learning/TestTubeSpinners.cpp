#include "TestTubeSpinners.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/misc/learning/TestLasers.h"
#include "Sandbox/misc/learning/TestTubeSpinnerProxy.h"
#include "Sandbox/misc/learning/TestTubeSpinnersConfig.h"
#include "Sandbox/utilities/actor_utils.h"

#include <SandboxCore/actor_utils.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/uobject_utils.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Engine/StaticMesh.h>
#include <EngineUtils.h>

ATestTubeSpinners::ATestTubeSpinners()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    ml::set_actor_component_mobility(*this, EComponentMobility::Static);
}

// Actor life cycle
void ATestTubeSpinners::clear_runtime_state() {
    instances->ClearInstances();
    ml::reset_arrays(transforms,
                     yaws,
                     laser_cooldowns,
                     next_fire_point_indices,
                     indices_ready_to_fire,
                     new_laser_transforms);
}
void ATestTubeSpinners::BeginPlay() {
    Super::BeginPlay();

    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(actor_config),
        SANDBOX_NAMED_UOBJECT_PTR(laser_actor),
    });

    clear_runtime_state();
    configure_ismc();
    register_all_proxies_in_level();

    ml::fatal_if_uobject_ptrs_invalid({
        {instances->GetStaticMesh().Get(), TEXT("ISMC Static Mesh")},
    });
}
void ATestTubeSpinners::Tick(float dt) {
    Super::Tick(dt);

    tick(dt);
}
void ATestTubeSpinners::tick(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::tick);

    laser_cooldowns.tick(dt);
    rotate_instances(dt);
    update_ismc();
    fire_lasers();
}

// Accessors
auto ATestTubeSpinners::get_num_instances() const noexcept -> int32 {
    return laser_cooldowns.Num();
}

// Spawning
void ATestTubeSpinners::register_all_proxies_in_level() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::register_all_proxies_in_level);

    auto* world{GetWorld()};

    if (!world) {
        return;
    }

    TArray<Proxy*> proxies{};
    ml::append_actors(*world, proxies);
    auto const n_to_add{proxies.Num()};

    TArray<FTransform> new_transforms;
    new_transforms.AddUninitialized(n_to_add);
    TArray<int32> new_fire_point_indices;
    new_fire_point_indices.AddUninitialized(n_to_add);

    for (int32 i{0}; i < n_to_add; ++i) {
        auto* proxy{proxies[i]};
        new_transforms[i] = proxy->GetActorTransform();
        new_fire_point_indices[i] = proxy->get_initial_active_fire_point();
    }

    spawn_instances(new_transforms, new_fire_point_indices);

    for (auto* proxy : proxies) {
        proxy->Destroy();
    }
}
void ATestTubeSpinners::spawn_instances(TConstArrayView<FTransform> const new_transforms,
                                        TConstArrayView<int32> const new_fire_point_indices) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::spawn_instances);

    auto const n{new_transforms.Num()};
    auto const existing_total{get_num_instances()};

    check(n == new_fire_point_indices.Num());

    transforms.AddUninitialized(n);
    yaws.AddUninitialized(n);
    next_fire_point_indices.Append(new_fire_point_indices);
    laser_cooldowns.AddZeroed(n);

    instances->AddInstances(TArray<FTransform>{new_transforms}, false, is_world_space, false);
    for (int32 i{0}; i < n; ++i) {
        auto const index{existing_total + i};

        auto const& transform{new_transforms[i]};
        transforms[index] = transform;
        yaws[index] = transform.GetRotation().Rotator().Yaw;
    }

    check(array_sizes_consistent());
}

// Movement
void ATestTubeSpinners::rotate_instances(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::rotate_instances);

    auto const speed{actor_config->yaw_rotation_speed_degrees};
    auto const delta_yaw_degrees{dt * speed};
    auto const n{get_num_instances()};

    for (int32 i{0}; i < n; ++i) {
        yaws[i] += delta_yaw_degrees;
    }

    for (int32 i{0}; i < n; ++i) {
        transforms[i].SetRotation(FRotator{0.f, yaws[i], 0.f}.Quaternion());
    }
}

// Visuals
void ATestTubeSpinners::configure_ismc() {
    instances->SetStaticMesh(actor_config->mesh);
    instances->SetCanEverAffectNavigation(false);
}
void ATestTubeSpinners::update_ismc() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::update_ismc);

    instances->BatchUpdateInstancesTransforms(0, transforms, is_world_space, true, false);
}

// Firing
void ATestTubeSpinners::fire_lasers() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::fire_lasers);

    auto const n{get_num_instances()};
    auto const cooldown{actor_config->attack_cooldown};
    auto const& firing_point_offsets{actor_config->fire_point_offsets};
    auto const n_firing_points{firing_point_offsets.Num()};

    if (n_firing_points < 1) {
        return;
    }

    indices_ready_to_fire.Reset();
    new_laser_transforms.Reset();

    for (int32 i{0}; i < n; ++i) {
        if (!(laser_cooldowns[i] <= 0.f)) {
            continue;
        }

        indices_ready_to_fire.Add(i);
        laser_cooldowns[i] = cooldown;
    }

    auto const n_ready_to_fire{indices_ready_to_fire.Num()};
    new_laser_transforms.Reserve(n_ready_to_fire);

    for (int32 i{0}; i < n_ready_to_fire; ++i) {
        auto const index{indices_ready_to_fire[i]};

        auto const& base_transform{transforms[index]};
        auto const fire_point_index{next_fire_point_indices[index]};
        new_laser_transforms.Add(firing_point_offsets[fire_point_index] * base_transform);

        next_fire_point_indices[index] = (fire_point_index + 1) % n_firing_points;
    }

    laser_actor->spawn_lasers(new_laser_transforms);
}

// Debugging
bool ATestTubeSpinners::array_sizes_consistent() const {
    return ml::all_num_equal(
        *instances, transforms, yaws, laser_cooldowns, next_fire_point_indices);
}
