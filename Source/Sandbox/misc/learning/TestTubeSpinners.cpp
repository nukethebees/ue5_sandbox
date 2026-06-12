#include "TestTubeSpinners.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/misc/learning/TestLasers.h"
#include "Sandbox/misc/learning/TestTubeSpinnerProxy.h"
#include "Sandbox/misc/learning/TestTubeSpinnersConfig.h"
#include "Sandbox/utilities/actor_utils.h"

#include <SandboxCore/actor_utils.h>
#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/uobject_utils.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Engine/StaticMesh.h>
#include <EngineUtils.h>

ATestTubeSpinners::ATestTubeSpinners()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    ml::set_actor_component_mobility(*this, EComponentMobility::Static);
}

// Actor life cycle
void ATestTubeSpinners::clear_runtime_state() {
    instances->ClearInstances();
    ml::reset(locations,
              yaws,
              laser_cooldowns,
              next_fire_point_indices,
              indices_ready_to_fire,
              new_laser_locations,
              new_laser_rotations);
}
void ATestTubeSpinners::begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::begin_play);

    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(actor_config),
        SANDBOX_NAMED_UOBJECT_PTR(laser_actor),
    });

    configure_ismc();
    register_all_proxies_in_level();

    ml::fatal_if_uobject_ptrs_invalid({
        {instances->GetStaticMesh().Get(), TEXT("ISMC Static Mesh")},
    });
}
void ATestTubeSpinners::tick(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::tick);

    laser_cooldowns.tick(dt);
    rotate_instances(dt);
    fire_lasers();
}
void ATestTubeSpinners::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::update_entity_registry);
}
void ATestTubeSpinners::update_visuals() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::update_visuals);
    update_ismc();
}
void ATestTubeSpinners::end_frame() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::end_frame);
}

// Accessors
auto ATestTubeSpinners::get_num_instances() const noexcept -> int32 {
    return laser_cooldowns.Num();
}

void ATestTubeSpinners::set_owner_id(TestEntityOwnerId const new_owner_id) {
    owner_id = new_owner_id;
}
auto ATestTubeSpinners::get_owner_id() const -> TestEntityOwnerId {
    return owner_id;
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

    FVectors3f new_locations;
    TArray<float> new_yaws;
    TArray<int32> new_fire_point_indices;

    ml::add_uninitialised(n_to_add, new_locations, new_yaws, new_fire_point_indices);

    for (int32 i{0}; i < n_to_add; ++i) {
        auto* proxy{proxies[i]};
        auto const& transform{proxy->GetActorTransform()};

        ml::assign(new_locations, i, transform.GetLocation());
        new_yaws[i] = transform.Rotator().Yaw;
        new_fire_point_indices[i] = proxy->get_initial_active_fire_point();
    }

    spawn_instances(new_locations.get_const_view(), new_yaws, new_fire_point_indices);

    for (auto* proxy : proxies) {
        proxy->Destroy();
    }
}
void ATestTubeSpinners::spawn_instances(FVectors3f::ConstView const new_locations,
                                        TConstArrayView<float> const new_yaws,
                                        TConstArrayView<int32> const new_fire_point_indices) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::spawn_instances);

    auto const n{new_locations.num()};
    auto const existing_total{get_num_instances()};

    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(new_locations),
        SANDBOX_NAMED_NUM(new_yaws),
        SANDBOX_NAMED_NUM(new_fire_point_indices),
    });

    ml::append_from(locations, new_locations);
    yaws.Append(new_yaws);
    laser_cooldowns.AddZeroed(n);
    next_fire_point_indices.Append(new_fire_point_indices);

    update_ismc_transforms();
    instances->AddInstances(TArray<FTransform>{ismc_transforms.GetData() + existing_total, n},
                            false,
                            is_world_space,
                            false);

    validate_array_sizes();
}

// Movement
void ATestTubeSpinners::rotate_instances(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::rotate_instances);

    auto const speed{actor_config->yaw_rotation_speed_degrees};
    auto const delta_yaw_degrees{dt * speed};
    auto const n{get_num_instances()};

    ml::add_in_place(TArrayView<float>(yaws), delta_yaw_degrees);
}

// Visuals
void ATestTubeSpinners::configure_ismc() {
    instances->SetStaticMesh(actor_config->mesh);
    instances->SetCanEverAffectNavigation(false);
}
void ATestTubeSpinners::update_ismc_transforms() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::update_ismc_transforms);

    auto const n{get_num_instances()};
    ismc_transforms.Reset();
    ismc_transforms.AddUninitialized(n);

    for (int32 i{0}; i < n; ++i) {
        ismc_transforms[i] = FTransform{
            FRotator{0.0, static_cast<double>(yaws[i]), 0.0},
            ml::get_vector3d(locations, i),
        };
    }
}
void ATestTubeSpinners::update_ismc() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::update_ismc);

    update_ismc_transforms();
    instances->BatchUpdateInstancesTransforms(0, ismc_transforms, is_world_space, true, false);
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

    ml::reset(indices_ready_to_fire, new_laser_locations, new_laser_rotations);

    for (int32 i{0}; i < n; ++i) {
        if (!(laser_cooldowns[i] <= 0.f)) {
            continue;
        }

        indices_ready_to_fire.Add(i);
        laser_cooldowns[i] = cooldown;
    }

    auto const n_ready_to_fire{indices_ready_to_fire.Num()};
    ml::reserve(n_ready_to_fire, new_laser_locations, new_laser_rotations);
    ml::add_uninitialised(new_laser_locations, n_ready_to_fire);
    ml::add_uninitialised(new_laser_rotations, n_ready_to_fire);

    for (int32 i{0}; i < n_ready_to_fire; ++i) {
        auto const index{indices_ready_to_fire[i]};

        auto const fire_point_index{next_fire_point_indices[index]};
        auto const& offset{firing_point_offsets[fire_point_index]};

        auto const fire_point_location{offset.GetLocation()};
        ml::assign(new_laser_locations,
                   i,
                   locations.xs[index] + fire_point_location.X,
                   locations.ys[index] + fire_point_location.Y,
                   locations.zs[index] + fire_point_location.Z);

        auto const fire_point_rotation{offset.Rotator()};
        ml::assign(new_laser_rotations,
                   i,
                   fire_point_rotation.Pitch,
                   fire_point_rotation.Yaw + yaws[index],
                   fire_point_rotation.Roll);

        next_fire_point_indices[index] = (fire_point_index + 1) % n_firing_points;
    }

    laser_actor->spawn_lasers(new_laser_locations.get_const_view(),
                              new_laser_rotations.get_const_view());
}

// Checks
void ATestTubeSpinners::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(locations),
        SANDBOX_NAMED_NUM(yaws),
        SANDBOX_NAMED_NUM(laser_cooldowns),
        SANDBOX_NAMED_NUM(next_fire_point_indices),
    });

    auto const n{get_num_instances()};
    auto const n_ismc{instances->GetNumInstances()};
    if (n_ismc < n) {
        UE_LOG(LogSandbox,
               Fatal,
               TEXT("ATestTubeSpinners::validate_array_sizes %d entities, %d ISMC instances"),
               n,
               n_ismc);
    }
}
