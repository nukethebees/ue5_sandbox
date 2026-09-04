#include "SpaceGame/defences/spinners/TestTubeSpinners.h"

#include <SandboxGameShared/utilities/actor_utils.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/entities/TestBatchActorCore.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>
#include <SpaceGame/support/mesh.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCoreEngine/actor_utils.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <Engine/StaticMesh.h>

ATestTubeSpinners::ATestTubeSpinners()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    ml::set_actor_component_mobility(*this, EComponentMobility::Static);
}

void ATestTubeSpinners::set_actor_config(FTubeSpinnerConfig const* const new_config) noexcept {
    actor_config = new_config;
    if (bound_simulation && actor_config) {
        bound_simulation->set_config(*actor_config);
    }
}

void ATestTubeSpinners::bind_simulation(ml::test_tube_spinners::Simulation& new_simulation) {
    bound_simulation = &new_simulation;
    if (actor_config) {
        bound_simulation->set_config(*actor_config);
    }
}

auto ATestTubeSpinners::simulation() -> ml::test_tube_spinners::Simulation& {
    check(bound_simulation);
    return *bound_simulation;
}

auto ATestTubeSpinners::simulation() const -> ml::test_tube_spinners::Simulation const& {
    return const_cast<ATestTubeSpinners*>(this)->simulation();
}

void ATestTubeSpinners::clear_runtime_state_presentation() {
    instances->ClearInstances();
    ismc_transforms.Reset();
}

void ATestTubeSpinners::begin_play_presentation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::begin_play_presentation);
    if (!actor_config) {
        UE_LOG(LogSandbox, Fatal, TEXT("ATestTubeSpinners actor_config is nullptr."));
    }
    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(instances.Get()),
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->mesh.Get()),
    });

    configure_ismc();
    simulation().entity_radius = ml::get_mesh_sphere_bounds(*instances);
    register_all_proxies_in_level();
    validate_array_sizes();
}

void ATestTubeSpinners::update_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::update_visual_data);
    update_ismc();
}

void ATestTubeSpinners::commit_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::commit_visual_data);
    instances->MarkRenderStateDirty();
}

void ATestTubeSpinners::end_tick_presentation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::end_tick_presentation);
    validate_array_sizes();
}

void ATestTubeSpinners::register_all_proxies_in_level() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::register_all_proxies_in_level);

    auto* world{GetWorld()};

    check(world);

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

    auto& spinner_simulation{simulation()};
    auto const existing_total{spinner_simulation.get_num_instances()};
    spinner_simulation.spawn_instances(
        new_locations.get_const_view(), new_yaws, new_fire_point_indices);

    update_ismc_transforms();
    if (n_to_add > 0) {
        instances->AddInstances(
            TArray<FTransform>{ismc_transforms.GetData() + existing_total, n_to_add},
            false,
            is_world_space,
            false);
    }
    auto const& entities{spinner_simulation.entities};

    auto const first_new_handle{entities.handles.Num() - n_to_add};
    for (int32 i{0}; i < n_to_add; ++i) {
        proxies[i]->set_entity_handle(entities.handles[first_new_handle + i]);
    }
}

void ATestTubeSpinners::configure_ismc() {
    ml::batch::configure_ismc(*instances, {.mesh = actor_config->mesh.Get()});
}

void ATestTubeSpinners::update_ismc_transforms() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::update_ismc_transforms);

    auto const& spinner_simulation{simulation()};
    auto const& entities{spinner_simulation.entities};
    auto const n{spinner_simulation.get_num_instances()};
    ismc_transforms.Reset();
    ismc_transforms.AddUninitialized(n);

    for (int32 i{0}; i < n; ++i) {
        ismc_transforms[i] = FTransform{
            FRotator{0.0, static_cast<double>(entities.yaws[i]), 0.0},
            ml::get_vector3d(entities.locations, i),
        };
    }
}

void ATestTubeSpinners::update_ismc() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestTubeSpinners::update_ismc);

    update_ismc_transforms();
    constexpr bool mark_render_state_dirty{false};
    constexpr bool teleport{false};
    instances->BatchUpdateInstancesTransforms(
        0, ismc_transforms, is_world_space, mark_render_state_dirty, teleport);
}

void ATestTubeSpinners::validate_array_sizes() const {
    simulation().validate_array_sizes();
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(simulation().get_num_instances()),
        SANDBOX_NAMED_NUM(ismc_transforms),
        SANDBOX_NAMED_NUM(instances->GetNumInstances()),
    });
}
