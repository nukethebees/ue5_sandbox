#include "SpaceGamePresentation/presentation/SpinnerPresentation.h"

#include <SandboxGameShared/utilities/actor_utils.h>
#include <SpaceGamePresentation/entities/TestBatchActorCore.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <Engine/StaticMesh.h>

FSpinnerPresentation::FSpinnerPresentation(UInstancedStaticMeshComponent& component)
    : instances{&component} {}

void FSpinnerPresentation::set_actor_config(FTubeSpinnerConfig const* const new_config) noexcept {
    actor_config = new_config;
}

void FSpinnerPresentation::clear_runtime_state_presentation() {
    instances->ClearInstances();
    ismc_transforms.Reset();
}

void FSpinnerPresentation::begin_play_presentation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FSpinnerPresentation::begin_play_presentation);
    if (!actor_config) {
        UE_LOG(LogSandbox, Fatal, TEXT("FSpinnerPresentation actor_config is nullptr."));
    }
    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(instances),
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->mesh.Get()),
    });

    configure_ismc();
    update_ismc_transforms();
    if (!ismc_transforms.IsEmpty()) {
        instances->AddInstances(ismc_transforms, false, is_world_space, false);
    }
    validate_array_sizes();
}

void FSpinnerPresentation::update_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FSpinnerPresentation::update_visual_data);
    update_ismc();
}

void FSpinnerPresentation::commit_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FSpinnerPresentation::commit_visual_data);
    instances->MarkRenderStateDirty();
}

void FSpinnerPresentation::end_tick_presentation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FSpinnerPresentation::end_tick_presentation);
    validate_array_sizes();
}

void FSpinnerPresentation::configure_ismc() {
    ml::batch::configure_ismc(*instances, {.mesh = actor_config->mesh.Get()});
}

void FSpinnerPresentation::update_ismc_transforms() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FSpinnerPresentation::update_ismc_transforms);

    auto const& spinner_simulation{view()};
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

void FSpinnerPresentation::update_ismc() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FSpinnerPresentation::update_ismc);

    update_ismc_transforms();
    constexpr bool mark_render_state_dirty{false};
    constexpr bool teleport{false};
    instances->BatchUpdateInstancesTransforms(
        0, ismc_transforms, is_world_space, mark_render_state_dirty, teleport);
}

void FSpinnerPresentation::validate_array_sizes() const {
    view().entities.validate_array_sizes();
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(view().get_num_instances()),
        SANDBOX_NAMED_NUM(ismc_transforms),
        SANDBOX_NAMED_NUM(instances->GetNumInstances()),
    });
}
