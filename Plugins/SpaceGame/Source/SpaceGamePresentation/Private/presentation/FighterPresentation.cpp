#include "SpaceGamePresentation/presentation/FighterPresentation.h"

#include <ioj/column_math.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>
#include <SpaceGamePresentation/entities/TestTeamVisualData.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCoreEngine/uobject_utils.h>
#include <SandboxCoreEngine/VectorConversion.h>

#include <Components/SceneComponent.h>
#include <Engine/StaticMesh.h>
#include <Engine/World.h>
#include <SandboxISMCComponent.h>

namespace ioj {
FFighterPresentation::FFighterPresentation(USandboxISMCComponent& component)
    : instances{&component} {}

void FFighterPresentation::set_actor_config(FFighterConfig const* const new_config) noexcept {
    actor_config = new_config;
}

void FFighterPresentation::clear_runtime_state_presentation() {
    instances->clear_instances();
}

void FFighterPresentation::begin_play_presentation(presentation::MissionFrame const& frame) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FFighterPresentation::begin_play_presentation);

    if (!actor_config) {
        UE_LOG(LogSandbox, Fatal, TEXT("FFighterPresentation actor_config is nullptr."));
    }
    fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->mesh.Get()),
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->team_visual_data.Get()),
    });

    configure_ismc();
    team_colours_ = actor_config->team_visual_data->build_team_colour_cache();

    debug_drawer = actor_config->debug_drawer;
    debug_drawer.world = instances->GetWorld();
    update_visual_data(frame);
    commit_visual_data(frame);
}

void FFighterPresentation::update_visual_data(presentation::MissionFrame const& frame) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FFighterPresentation::update_visual_data);
    update_ismc(frame);
}

void FFighterPresentation::commit_visual_data(presentation::MissionFrame const& frame) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FFighterPresentation::commit_visual_data);
    if (enable_target_debug_drawing || enable_ship_location_debug_drawing) {
        draw_debug_shapes(frame);
    }
}

void FFighterPresentation::configure_ismc() {
    instances->SetMobility(EComponentMobility::Movable);
    instances->set_static_mesh(*actor_config->mesh);

    instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    instances->SetGenerateOverlapEvents(false);
    instances->SetCanEverAffectNavigation(false);
    instances->SetCastShadow(false);
    instances->SetAffectDistanceFieldLighting(false);
    instances->SetReceivesDecals(false);
    instances->set_num_custom_data_floats(n_custom_ismc_floats);
}

void FFighterPresentation::update_ismc(presentation::MissionFrame const& frame) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FFighterPresentation::update_ismc);

    auto const& data{frame.fighters()};
    auto const count{static_cast<int32>(data.num())};
    auto const ids{data.ids};
    visible_indices_.Reset();
    for (int32 index{}; index < count; ++index) {
        if (sim::is_alive(data.healths.health(ids[index]))) {
            visible_indices_.Add(index);
        }
    }
    instances->set_instances(
        visible_indices_.Num(),
        ESandboxISMCParallelism::Auto,
        [this, &data, &frame](FSandboxISMCInstanceChunkWriter& chunk) {
            auto const first_index{chunk.first_index()};
            auto const chunk_count{chunk.num()};
            auto const locations{data.locations};
            auto const aim_directions{frame.fighter_aim};
            auto const teams{data.teams};

            TArray<FVector3f, TInlineAllocator<1024>> transform_positions;
            transform_positions.SetNumUninitialized(chunk.num());
            TArray<FQuat4f, TInlineAllocator<1024>> transform_rotations;
            transform_rotations.SetNumUninitialized(chunk.num());
            for (int32 local_index{0}; local_index < chunk_count; ++local_index) {
                auto const index{visible_indices_[first_index + local_index]};
                auto const position{to_unreal(vector_at(locations, index))};
                FVector const direction{to_unreal(vector_at(aim_directions, index))};
                auto const rotation{
                    FQuat4f{FQuat::FindBetweenNormals(FVector::ForwardVector, direction)}};
                transform_positions[local_index] = position;
                transform_rotations[local_index] = rotation;

                auto custom_data{chunk.custom_data(local_index)};
                auto const& colour{team_colours_[to_unreal(teams[index])]};
                custom_data[0] = colour.R;
                custom_data[1] = colour.G;
                custom_data[2] = colour.B;
            }
            chunk.set_transforms<ESandboxISMCBoundsMode::Calculate>(transform_positions,
                                                                    transform_rotations);
        });
}

void FFighterPresentation::draw_debug_shapes(presentation::MissionFrame const& frame) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FFighterPresentation::draw_debug_shapes);

    auto const& data{frame.fighters()};
    auto const n{static_cast<int32>(data.num())};
    auto const locations{data.locations};
    auto const targets{frame.fighter_targets.ids};
    auto const target_locations{frame.fighter_targets.locations};

    for (int32 i{0}; i < n; ++i) {
        FVector const ship_location{to_unreal(vector_at(locations, i))};
        if (enable_ship_location_debug_drawing) {
            debug_drawer.draw_sphere(ship_location);
        }
        if (enable_target_debug_drawing && targets[i].is_valid()) {
            debug_drawer.draw_line(ship_location,
                                   FVector{to_unreal(vector_at(target_locations, i))});
        }
    }
}

} // namespace ioj
