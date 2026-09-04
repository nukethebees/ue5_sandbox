#include "SpaceGame/defences/spinners/TestTubeSpinners.h"

#include <SandboxGameShared/utilities/actor_utils.h>
#include <SpaceGame/entities/TestBatchActorCore.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_vector_utils.h>
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
}

void ATestTubeSpinners::bind_simulation(ml::test_tube_spinners::Simulation& new_simulation) {
    bound_simulation = &new_simulation;
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
    update_ismc_transforms();
    if (!ismc_transforms.IsEmpty()) {
        instances->AddInstances(ismc_transforms, false, is_world_space, false);
    }
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
