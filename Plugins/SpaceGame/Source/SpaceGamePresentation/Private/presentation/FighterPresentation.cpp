#include "SpaceGamePresentation/presentation/FighterPresentation.h"

#include <SandboxGameShared/utilities/actor_utils.h>
#include <SpaceGamePresentation/entities/TestBatchActorCore.h>
#include <SpaceGamePresentation/entities/TestTeamVisualData.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Async/ParallelFor.h>
#include <Components/SceneComponent.h>
#include <Engine/StaticMesh.h>

FFighterPresentation::FFighterPresentation(UInstancedStaticMeshComponent& component)
    : instances{&component} {}

void FFighterPresentation::set_actor_config(FFighterConfig const* const new_config) noexcept {
    actor_config = new_config;
}

void FFighterPresentation::clear_runtime_state_presentation() {
    instances->ClearInstances();
    ismc_transforms.Reset();
    dummy_transforms_spawn_buffer.Reset();
    custom_data_buffer.Reset();
}

void FFighterPresentation::begin_play_presentation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FFighterPresentation::begin_play_presentation);

    if (!actor_config) {
        UE_LOG(LogSandbox, Fatal, TEXT("FFighterPresentation actor_config is nullptr."));
    }
    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->mesh.Get()),
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->team_visual_data.Get()),
    });

    configure_ismc();

    debug_drawer = actor_config->debug_drawer;
    debug_drawer.world = instances->GetWorld();
    update_visual_data();
    commit_visual_data();
}

void FFighterPresentation::update_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FFighterPresentation::update_visual_data);
    apply_simulation_changes_to_ismc();
    prepare_ismc_transforms();
    update_ismc();
}

void FFighterPresentation::commit_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FFighterPresentation::commit_visual_data);
    instances->MarkRenderStateDirty();
    if (enable_target_debug_drawing || enable_ship_location_debug_drawing) {
        draw_debug_shapes();
    }
}

void FFighterPresentation::end_tick_presentation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FFighterPresentation::end_tick_presentation);
    validate_array_sizes();
}

void FFighterPresentation::configure_ismc() {
    ml::batch::configure_ismc(*instances,
                              {
                                  .mesh = actor_config->mesh.Get(),
                                  .num_custom_data_floats = n_custom_ismc_floats,
                              });
}

void FFighterPresentation::apply_simulation_changes_to_ismc() {
    auto const count{view().get_num_instances()};
    auto const previous_count{instances->GetNumInstances()};
    if (count < previous_count) {
        TArray<int32> indices;
        indices.Reserve(previous_count - count);
        for (int32 i{previous_count - 1}; i >= count; --i) {
            indices.Add(i);
        }
        instances->RemoveInstances(indices, true);
    } else if (count > previous_count) {
        dummy_transforms_spawn_buffer.SetNum(count - previous_count, EAllowShrinking::No);
        instances->AddInstances(dummy_transforms_spawn_buffer, false, is_world_space, false);
    }
    ismc_transforms.SetNum(count, EAllowShrinking::No);
    write_ismc_custom_data(0, count);
}

void FFighterPresentation::prepare_ismc_transforms() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FFighterPresentation::prepare_ismc_transforms);

    auto const& data{view().entities};
    auto const n{data.num()};
    ismc_transforms.SetNum(n, EAllowShrinking::No);
    constexpr int32 n_jobs{8};
    auto const updates_per_slice{FMath::DivideAndRoundUp(n, n_jobs)};
    ParallelFor(n_jobs, [this, &data, updates_per_slice, n](int32 const job_index) {
        auto const begin{job_index * updates_per_slice};
        auto const end{FMath::Min(begin + updates_per_slice, n)};
        for (int32 i{begin}; i < end; ++i) {
            ismc_transforms[i].SetLocation(ml::get_vector3d(data.locations, i));
            auto const direction{ml::get_vector3d(data.aim_directions, i)};
            ismc_transforms[i].SetRotation(
                FQuat::FindBetweenNormals(FVector::ForwardVector, direction));
        }
    });
}

void FFighterPresentation::update_ismc() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FFighterPresentation::update_ismc);
    constexpr bool mark_render_state_dirty{false};
    constexpr bool teleport{true};
    instances->BatchUpdateInstancesTransforms(
        0, ismc_transforms, is_world_space, mark_render_state_dirty, teleport);
}

void FFighterPresentation::draw_debug_shapes() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FFighterPresentation::draw_debug_shapes);

    auto const& data{view().entities};
    auto const n{data.num()};
    for (int32 i{0}; i < n; ++i) {
        FVector const ship_location{ml::get_vector3d(data.locations, i)};
        if (enable_ship_location_debug_drawing) {
            debug_drawer.draw_sphere(ship_location);
        }
        if (enable_target_debug_drawing && data.target_handles[i].is_valid()) {
            debug_drawer.draw_line(ship_location, ml::get_vector3d(data.target_locations, i));
        }
    }
}

void FFighterPresentation::write_ismc_custom_data(int32 const offset, int32 const count) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FFighterPresentation::write_ismc_custom_data);
    check(count >= 0);
    if (count == 0) {
        return;
    }

    auto const& data{view().entities};
    auto const colour_cache{
        UTestTeamVisualData::build_team_colour_cache(actor_config->team_visual_data)};
    custom_data_buffer.SetNumUninitialized(count * n_custom_ismc_floats, EAllowShrinking::No);
    auto const teams_slice{TConstArrayView<ETestTeam>{data.teams}.Slice(offset, count)};
    for (int32 i{0}; i < count; ++i) {
        auto const base{i * n_custom_ismc_floats};
        auto const& colour{colour_cache[teams_slice[i]]};
        custom_data_buffer[base + 0] = colour.R;
        custom_data_buffer[base + 1] = colour.G;
        custom_data_buffer[base + 2] = colour.B;
    }

    constexpr bool mark_render_dirty{false};
    instances->SetCustomData(offset, offset + count - 1, custom_data_buffer, mark_render_dirty);
}

void FFighterPresentation::validate_array_sizes() const {
    view().entities.validate_array_sizes();
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(view().get_num_instances()),
        SANDBOX_NAMED_NUM(ismc_transforms),
        SANDBOX_NAMED_NUM(instances->GetNumInstances()),
    });
}
