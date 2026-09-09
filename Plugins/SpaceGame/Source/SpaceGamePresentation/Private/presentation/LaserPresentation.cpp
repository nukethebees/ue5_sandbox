#include "SpaceGamePresentation/presentation/LaserPresentation.h"

#include <SandboxGameShared/utilities/actor_utils.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxISMCComponent.h>
#include <SpaceGameRendering/SparkEffects.h>

#include <Components/SceneComponent.h>
#include <ProfilingDebugging/CountersTrace.h>

TRACE_DECLARE_INT_COUNTER(SandboxTestLaserISMCCount, TEXT("Sandbox/TestLaserISMCCount"));

namespace SpaceGame::LaserPresentation::Private {
auto make_seed(uint64 const tick, FVector3f const location, int32 const ordinal) -> uint32 {
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
    material_data.Reset();
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
    material_data.Reserve(actor_config->n_preallocated_instances);

#if WITH_EDITOR
    debug_drawer.world = instances->GetWorld();
#endif

    update_visual_data();
    validate_array_sizes();
}

void FLaserPresentation::update_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLaserPresentation::update_visual_data);
    synchronize_material_data();
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
    instances->SetMobility(EComponentMobility::Static);
    instances->SetMaterial(0, actor_config->material);

    instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    instances->SetGenerateOverlapEvents(false);
    instances->SetCanEverAffectNavigation(false);
    instances->SetCastShadow(false);
    instances->SetAffectDistanceFieldLighting(false);
    instances->SetReceivesDecals(false);
    instances->set_num_custom_data_floats(n_custom_ismc_floats);
}

auto FLaserPresentation::source_colour(FLaserSource const source) const -> FLinearColor {
    switch (source.type) {
        case ETestEntityType::PlayerShip:
            return player_colours_[source.team];
        case ETestEntityType::CapitalShipFighter:
            return fighter_colours_[source.team];
        case ETestEntityType::Turret:
            return turret_colours_[source.team];
        default:
            return FLinearColor::White;
    }
}
void FLaserPresentation::synchronize_material_data() {
    auto const& entities{view().entities};
    auto const count{entities.num()};
    material_data.SetNum(count, EAllowShrinking::No);
    for (int32 i{}; i < count; ++i) {
        auto const colour{source_colour(entities.sources[i])};
        material_data[i] = {
            {colour.R, colour.G, colour.B}, entities.initial_lifetimes[i], entities.spawn_times[i]};
    }
}

void FLaserPresentation::update_ismc() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLaserPresentation::update_ismc);

    auto const& laser_simulation{view()};
    auto const count{laser_simulation.get_num_instances()};
    instances->set_instances(
        count, ESandboxISMCParallelism::Auto, [this, &laser_simulation](auto& chunk) {
            auto const first_index{chunk.first_index()};
            auto const chunk_count{chunk.num()};
            for (int32 local_index{0}; local_index < chunk_count; ++local_index) {
                auto const index{first_index + local_index};
                auto const& locations{laser_simulation.entities.locations};
                auto const location{
                    FVector3f{locations.xs[index], locations.ys[index], locations.zs[index]}};
                auto const& rotations{laser_simulation.entities.rotations};
                auto const rotation{FRotator3f{
                    rotations.pitches[index], rotations.yaws[index], rotations.rolls[index]}
                                        .Quaternion()};
                chunk.set_transform(local_index, location, rotation, FVector3f::OneVector);

                auto custom_data{chunk.custom_data(local_index)};
                auto const& data{material_data[index]};
                custom_data[0] = data.colour.X;
                custom_data[1] = data.colour.Y;
                custom_data[2] = data.colour.Z;
                custom_data[3] = data.initial_lifetime;
                custom_data[4] = data.spawn_time;
            }
        });
}

void FLaserPresentation::queue_hit_sparks() {
    if (spark_effects_ == nullptr || actor_config == nullptr ||
        actor_config->impact_sparks.count <= 0) {
        return;
    }

    auto const& hit_details{view().hits};
    auto const count{ml::num(hit_details)};
    auto const& style{actor_config->impact_sparks};
    for (int32 index{0}; index < count; ++index) {
        auto const location{ml::get_vector3f(hit_details.locations, index)};
        auto const colour{source_colour(hit_details.sources[index])};
        spark_effects_->queue_burst({
            .emission =
                {
                    .location = location,
                    .direction = ml::get_vector3f(hit_details.emission_directions, index),
                    .colour = FVector3f{colour.R, colour.G, colour.B},
                    .seed = SpaceGame::LaserPresentation::Private::make_seed(
                        view().hit_ticks[index], location, view().hit_ordinals[index]),
                },
            .style = style,
        });
    }
}

void FLaserPresentation::validate_array_sizes() const {
    view().entities.validate_array_sizes();
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(view().get_num_instances()),
        SANDBOX_NAMED_NUM(material_data.Num()),
        SANDBOX_NAMED_NUM(instances->get_instance_count()),
    });
}
