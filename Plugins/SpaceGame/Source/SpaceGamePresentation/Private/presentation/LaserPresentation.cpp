#include "SpaceGamePresentation/presentation/LaserPresentation.h"

#include <ioj/sim/column_math.h>
#include <ioj/sim/entity_types.h>
#include <ioj/sim/laser_source.h>
#include <SandboxGameShared/utilities/actor_utils.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>
#include <SpaceGamePresentation/integration/VectorConversion.h>
#include <SpaceGameRendering/SparkEffects.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>

#include <Components/SceneComponent.h>
#include <ProfilingDebugging/CountersTrace.h>
#include <SandboxISMCComponent.h>

TRACE_DECLARE_INT_COUNTER(SandboxTestLaserISMCCount, TEXT("Sandbox/TestLaserISMCCount"));

namespace SpaceGame::LaserPresentation::Private {
auto make_seed(::ioj::sim::SimTick const tick, FVector3f const location, int32 const ordinal)
    -> uint32 {
    auto seed{HashCombineFast(GetTypeHash(static_cast<uint32>(tick)),
                              GetTypeHash(static_cast<uint32>(tick >> 32)))};
    seed = HashCombineFast(seed, GetTypeHash(location));
    return HashCombineFast(seed, GetTypeHash(ordinal));
}
} // namespace SpaceGame::LaserPresentation::Private

FLaserPresentation::FLaserPresentation(USandboxISMCComponent& component)
    : instances{&component} {}

void FLaserPresentation::clear_runtime_state_presentation() {
    instances->clear_instances();
    visible_indices_.Reset();
}

void FLaserPresentation::begin_play_presentation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLaserPresentation::begin_play_presentation);
    TRACE_COUNTER_SET(SandboxTestLaserISMCCount, 0);

    if (!actor_config) {
        UE_LOG(LogSandboxLearning, Fatal, TEXT("actor_config is nullptr."));
    }
    if (!actor_config->mesh || !actor_config->material) {
        UE_LOG(LogSandboxLearning, Fatal, TEXT("actor_config is not ready."));
    }

    configure_ismc();
    visible_indices_.Reserve(actor_config->n_preallocated_instances);
    instances->reserve_instances(actor_config->n_preallocated_instances);

#if WITH_EDITOR
    debug_drawer.world = instances->GetWorld();
#endif

    update_visual_data();
    validate_array_sizes();
}

void FLaserPresentation::update_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLaserPresentation::update_visual_data);
    update_ismc();
    queue_hit_sparks();
}

void FLaserPresentation::end_tick_presentation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLaserPresentation::end_tick_presentation);
    TRACE_COUNTER_SET(SandboxTestLaserISMCCount, instances->get_instance_count());
    validate_array_sizes();
}

void FLaserPresentation::configure_ismc() {
    instances->SetMobility(EComponentMobility::Movable);
    instances->set_static_mesh(*actor_config->mesh);
    check(instances->get_static_mesh() == actor_config->mesh);
    instances->SetMaterial(0, actor_config->material);

    instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    instances->SetGenerateOverlapEvents(false);
    instances->SetCanEverAffectNavigation(false);
    instances->SetCastShadow(false);
    instances->SetAffectDistanceFieldLighting(false);
    instances->SetReceivesDecals(false);
    instances->set_num_custom_data_floats(n_custom_ismc_floats);
}

auto FLaserPresentation::source_colour(::ioj::sim::LaserSource const source) const -> FLinearColor {
    switch (source.type) {
        case ::ioj::sim::EntityType::PlayerShip:
            return player_colours_[ml::to_unreal(source.team)];
        case ::ioj::sim::EntityType::Fighter:
            return fighter_colours_[ml::to_unreal(source.team)];
        case ::ioj::sim::EntityType::Turret:
            return turret_colours_[ml::to_unreal(source.team)];
        default:
            return FLinearColor::White;
    }
}
void FLaserPresentation::update_ismc() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLaserPresentation::update_ismc);
    auto const active{view().entities.active()};
    auto const count{view().entities.num()};
    visible_indices_.Reset();
    for (int32 index{}; index < count; ++index) {
        if (active[index] != 0) {
            visible_indices_.Add(index);
        }
    }
    instances->set_instances(visible_indices_.Num(),
                             ESandboxISMCParallelism::Auto,
                             [this](auto& chunk) { fill_chunk(chunk); });
}

void FLaserPresentation::fill_chunk(FSandboxISMCInstanceChunkWriter& chunk) const {
    auto const entities{view().entities};
    auto const first_index{chunk.first_index()};
    auto const chunk_count{chunk.num()};
    auto const locations{entities.view_locations()};
    auto const rotations{entities.view_rotations()};
    auto const pitches{rotations.pitches()};
    auto const yaws{rotations.yaws()};
    auto const rolls{rotations.rolls()};
    auto const sources{entities.sources()};
    auto const initial_lifetimes{entities.initial_lifetimes()};
    auto const spawn_times{entities.spawn_times()};
    for (int32 local_index{}; local_index < chunk_count; ++local_index) {
        auto const index{visible_indices_[first_index + local_index]};
        auto const location{
            FVector3f{locations.xs()[index], locations.ys()[index], locations.zs()[index]}};
        auto const rotation{FRotator3f{pitches[index], yaws[index], rolls[index]}.Quaternion()};
        chunk.set_transform(local_index, location, rotation, FVector3f::OneVector);

        auto const colour{source_colour(sources[index])};
        auto custom_data{chunk.custom_data(local_index)};
        custom_data[0] = colour.R;
        custom_data[1] = colour.G;
        custom_data[2] = colour.B;
        custom_data[3] = initial_lifetimes[index];
        custom_data[4] = spawn_times[index];
    }
}

void FLaserPresentation::queue_hit_sparks() {
    if (spark_effects_ == nullptr || actor_config == nullptr ||
        actor_config->impact_sparks.count <= 0) {
        return;
    }

    auto const& hit_details{view().hits};
    auto const count{ml::num(hit_details)};
    auto const& style{actor_config->impact_sparks};
    auto const locations{hit_details.view_locations()};
    auto const sources{hit_details.sources()};
    auto const directions{hit_details.view_emission_directions()};

    for (int32 index{0}; index < count; ++index) {
        auto const location{ml::to_unreal(::ioj::sim::vector_at(locations, index))};
        auto const colour{source_colour(sources[index])};
        spark_effects_->queue_burst({
            .emission =
                {
                    .location = location,
                    .direction = ml::to_unreal(::ioj::sim::vector_at(directions, index)),
                    .colour = FVector3f{colour.R, colour.G, colour.B},
                    .seed = SpaceGame::LaserPresentation::Private::make_seed(
                        view().hit_ticks[index], location, view().hit_ordinals[index]),
                },
            .style = style,
        });
    }
}

void FLaserPresentation::validate_array_sizes() const {
    view().entities.validate();
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(visible_indices_.Num()),
        SANDBOX_NAMED_NUM(instances->get_instance_count()),
    });
}
