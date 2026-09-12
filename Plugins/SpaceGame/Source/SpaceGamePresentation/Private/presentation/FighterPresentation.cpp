#include "SpaceGamePresentation/presentation/FighterPresentation.h"

#include <SpaceGamePresentation/entities/TestTeamVisualData.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCoreEngine/uobject_utils.h>
#include <SandboxISMCComponent.h>

#include <Components/SceneComponent.h>
#include <Engine/StaticMesh.h>
#include <Engine/World.h>

FFighterPresentation::FFighterPresentation(USandboxISMCComponent& component)
    : instances{&component} {}

void FFighterPresentation::set_actor_config(FFighterConfig const* const new_config) noexcept {
    actor_config = new_config;
}

void FFighterPresentation::clear_runtime_state_presentation() {
    instances->clear_instances();
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
    team_colours_ = actor_config->team_visual_data->build_team_colour_cache();

    debug_drawer = actor_config->debug_drawer;
    debug_drawer.world = instances->GetWorld();
    update_visual_data();
    commit_visual_data();
}

void FFighterPresentation::update_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FFighterPresentation::update_visual_data);
    update_ismc();
}

void FFighterPresentation::commit_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FFighterPresentation::commit_visual_data);
    if (enable_target_debug_drawing || enable_ship_location_debug_drawing) {
        draw_debug_shapes();
    }
}

void FFighterPresentation::end_tick_presentation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FFighterPresentation::end_tick_presentation);
    validate_array_sizes();
}

void FFighterPresentation::configure_ismc() {
    instances->SetMobility(EComponentMobility::Movable);
    instances->set_static_mesh(*actor_config->mesh);
    check(instances->get_static_mesh() == actor_config->mesh);
    instances->SetMobility(EComponentMobility::Static);

    instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    instances->SetGenerateOverlapEvents(false);
    instances->SetCanEverAffectNavigation(false);
    instances->SetCastShadow(false);
    instances->SetAffectDistanceFieldLighting(false);
    instances->SetReceivesDecals(false);
    instances->set_num_custom_data_floats(n_custom_ismc_floats);
}

void FFighterPresentation::update_ismc() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FFighterPresentation::update_ismc);

    auto const& data{view().entities};
    auto const count{data.num()};
    instances->set_instances(count, ESandboxISMCParallelism::Auto, [this, &data](auto& chunk) {
        auto const first_index{chunk.first_index()};
        auto const chunk_count{chunk.num()};
        for (int32 local_index{0}; local_index < chunk_count; ++local_index) {
            auto const index{first_index + local_index};
            auto const position{ml::get_vector3f(data.locations, index)};
            auto const direction{ml::get_vector3d(data.aim_directions, index)};
            auto const rotation{
                FQuat4f{FQuat::FindBetweenNormals(FVector::ForwardVector, direction)}};
            chunk.set_transform(local_index, position, rotation, FVector3f::OneVector);

            auto custom_data{chunk.custom_data(local_index)};
            auto const& colour{team_colours_[data.teams[index]]};
            custom_data[0] = colour.R;
            custom_data[1] = colour.G;
            custom_data[2] = colour.B;
        }
    });
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

void FFighterPresentation::validate_array_sizes() const {
    view().entities.validate_array_sizes();
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(view().get_num_instances()),
        SANDBOX_NAMED_NUM(instances->get_instance_count()),
    });
}
