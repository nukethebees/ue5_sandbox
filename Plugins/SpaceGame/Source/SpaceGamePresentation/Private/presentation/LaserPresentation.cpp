#include "SpaceGamePresentation/presentation/LaserPresentation.h"

#include <ioj/column_math.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/sim/lasers/laser_source.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>
#include <SpaceGameRendering/SparkEffects.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_utils.h>
#include <SandboxCoreEngine/VectorConversion.h>

#include <Components/SceneComponent.h>
#include <ProfilingDebugging/CountersTrace.h>
#include <SandboxISMCComponent.h>

TRACE_DECLARE_INT_COUNTER(SandboxTestLaserISMCCount, TEXT("Sandbox/TestLaserISMCCount"));

namespace SpaceGame::LaserPresentation::Private {
auto make_seed(::ioj::sim::SimTick const tick, FVector3f const location, int32 const ordinal)
    -> uint32 {
    auto seed{HashCombineFast(GetTypeHash(static_cast<uint32>(tick.value())),
                              GetTypeHash(static_cast<uint32>(tick.value() >> 32)))};
    seed = HashCombineFast(seed, GetTypeHash(location));
    return HashCombineFast(seed, GetTypeHash(ordinal));
}
} // namespace SpaceGame::LaserPresentation::Private

namespace ioj {
FLaserPresentation::FLaserPresentation(USandboxISMCComponent& component)
    : instances{&component} {}

void FLaserPresentation::clear_runtime_state_presentation() {
    instances->clear_instances();
    visible_indices_.Reset();
}

void FLaserPresentation::begin_play_presentation(presentation::LaserView const& lasers,
                                                 int32 reserve) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLaserPresentation::begin_play_presentation);
    TRACE_COUNTER_SET(SandboxTestLaserISMCCount, 0);

    if (!actor_config) {
        UE_LOG(LogSandboxLearning, Fatal, TEXT("actor_config is nullptr."));
    }
    if (!actor_config->mesh || !actor_config->material) {
        UE_LOG(LogSandboxLearning, Fatal, TEXT("actor_config is not ready."));
    }

    configure_ismc();
    visible_indices_.Reserve(reserve);
    instances->reserve_instances(reserve);

#if WITH_EDITOR
    debug_drawer.world = instances->GetWorld();
#endif

    update_visual_data(lasers, nullptr);
}

void FLaserPresentation::update_visual_data(presentation::LaserView const& lasers,
                                            presentation::FrameEvents const* impacts) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLaserPresentation::update_visual_data);
    update_ismc(lasers);
    if (impacts) {
        queue_hit_sparks(*impacts);
    }
}

void FLaserPresentation::end_tick_presentation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLaserPresentation::end_tick_presentation);
    TRACE_COUNTER_SET(SandboxTestLaserISMCCount, instances->get_instance_count());
}

void FLaserPresentation::configure_ismc() {
    instances->SetMobility(EComponentMobility::Movable);
    instances->set_static_mesh(*actor_config->mesh);
    instances->SetMaterial(0, actor_config->material);

    instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    instances->SetGenerateOverlapEvents(false);
    instances->SetCanEverAffectNavigation(false);
    instances->SetCastShadow(false);
    instances->SetAffectDistanceFieldLighting(false);
    instances->SetReceivesDecals(false);
    instances->set_num_custom_data_floats(n_custom_ismc_floats);
}

auto FLaserPresentation::source_colour(sim::LaserSource const source) const -> FLinearColor {
    switch (source.type) {
        case sim::EntityType::PlayerShip:
            return player_colours_[to_unreal(source.team)];
        case sim::EntityType::Fighter:
            return fighter_colours_[to_unreal(source.team)];
        case sim::EntityType::Turret:
            return turret_colours_[to_unreal(source.team)];
        default:
            return FLinearColor::White;
    }
}
void FLaserPresentation::update_ismc(presentation::LaserView const& lasers) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLaserPresentation::update_ismc);
    auto const active{lasers.active};
    auto const count{static_cast<int32>(lasers.active.size())};
    visible_indices_.Reset();
    for (int32 index{}; index < count; ++index) {
        if (active[index] != 0) {
            visible_indices_.Add(index);
        }
    }
    instances->set_instances(
        visible_indices_.Num(),
        ESandboxISMCParallelism::Auto,
        [this, &lasers](FSandboxISMCInstanceChunkWriter& chunk) { fill_chunk(lasers, chunk); });
}

void FLaserPresentation::fill_chunk(presentation::LaserView const& lasers,
                                    FSandboxISMCInstanceChunkWriter& chunk) const {
    auto const first_index{chunk.first_index()};
    auto const chunk_count{chunk.num()};
    auto const locations{lasers.locations};
    auto const rotations{lasers.rotations};
    auto const pitches{rotations.pitches()};
    auto const yaws{rotations.yaws()};
    auto const rolls{rotations.rolls()};
    auto const sources{lasers.sources};
    auto const initial_lifetimes{lasers.initial_lifetimes};
    auto const spawn_times{lasers.spawn_times};
    TArray<FVector3f, TInlineAllocator<1024>> transform_positions;
    transform_positions.SetNumUninitialized(chunk.num());
    TArray<FQuat4f, TInlineAllocator<1024>> transform_rotations;
    transform_rotations.SetNumUninitialized(chunk.num());
    for (int32 local_index{}; local_index < chunk_count; ++local_index) {
        auto const index{visible_indices_[first_index + local_index]};
        auto const location{
            FVector3f{locations.xs()[index], locations.ys()[index], locations.zs()[index]}};
        auto const rotation{FRotator3f{pitches[index], yaws[index], rolls[index]}.Quaternion()};
        transform_positions[local_index] = location;
        transform_rotations[local_index] = rotation;

        auto const colour{source_colour(sources[index])};
        auto custom_data{chunk.custom_data(local_index)};
        custom_data[0] = colour.R;
        custom_data[1] = colour.G;
        custom_data[2] = colour.B;
        custom_data[3] = initial_lifetimes[index];
        custom_data[4] = spawn_times[index];
    }
    chunk.set_transforms<ESandboxISMCBoundsMode::Calculate>(transform_positions,
                                                            transform_rotations);
}

void FLaserPresentation::queue_hit_sparks(presentation::FrameEvents const& events) {
    if (actor_config->impact_sparks.count <= 0) {
        return;
    }

    auto const& style{actor_config->impact_sparks};
    auto const locations{events.impacts.view_locations()};
    auto const sources{events.impacts.sources()};
    auto const directions{events.impacts.view_emission_directions()};

    std::uint32_t first{};
    auto tick{events.base_tick};
    for (auto const counts : events.ticks) {
        for (auto index{first}; index < counts.impacts; ++index) {
            auto const location{to_unreal(vector_at(locations, index))};
            auto const colour{source_colour(sources[index])};
            spark_effects_->queue_burst({
                .emission =
                    {
                        .location = Vector3f{location},
                        .direction = vector_at(directions, index),
                        .colour = {colour.R, colour.G, colour.B},
                        .seed = SpaceGame::LaserPresentation::Private::make_seed(
                            tick, location, static_cast<int32>(index - first)),
                    },
                .style = style.to_native(),
            });
        }
        first = counts.impacts;
        tick.set_value(tick.value() + 1);
    }
}

} // namespace ioj
