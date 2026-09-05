#include "SpaceGame/presentation/LaserPresentation.h"

#include <SandboxGameShared/utilities/actor_utils.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxISMCComponent.h>

#include <Components/SceneComponent.h>
#include <NiagaraFunctionLibrary.h>
#include <ProfilingDebugging/CountersTrace.h>

TRACE_DECLARE_INT_COUNTER(SandboxTestLaserISMCCount, TEXT("Sandbox/TestLaserISMCCount"));

FLaserPresentation::FLaserPresentation(USandboxISMCComponent& component)
    : instances{&component} {}

void FLaserPresentation::bind_simulation(ml::test_lasers::Simulation& new_simulation) {
    bound_simulation = &new_simulation;
}

auto FLaserPresentation::simulation() -> ml::test_lasers::Simulation& {
    check(bound_simulation);
    return *bound_simulation;
}

auto FLaserPresentation::simulation() const -> ml::test_lasers::Simulation const& {
    return const_cast<FLaserPresentation*>(this)->simulation();
}

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

    validate_array_sizes();
}

void FLaserPresentation::update_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLaserPresentation::update_visual_data);
    synchronize_material_data();
    update_ismc();
}

void FLaserPresentation::commit_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLaserPresentation::commit_visual_data);
    spawn_hit_effects();
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

void FLaserPresentation::synchronize_material_data() {
    auto const& laser_simulation{simulation()};
    auto const spawn_count{laser_simulation.presentation_spawn_count};
    auto const& custom_data{laser_simulation.presentation_custom_data_to_add};
    check(custom_data.Num() == spawn_count * n_custom_ismc_floats);
    for (int32 index{0}; index < spawn_count; ++index) {
        auto const offset{index * n_custom_ismc_floats};
        material_data.Add(
            {.colour = {custom_data[offset], custom_data[offset + 1], custom_data[offset + 2]},
             .initial_lifetime = custom_data[offset + 3],
             .spawn_time = custom_data[offset + 4]});
    }

    for (auto const index : laser_simulation.presentation_indices_to_remove) {
        material_data.RemoveAtSwap(index, EAllowShrinking::No);
    }

    check(material_data.Num() == laser_simulation.get_num_instances());
}

void FLaserPresentation::update_ismc() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLaserPresentation::update_ismc);

    auto const& laser_simulation{simulation()};
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

void FLaserPresentation::spawn_hit_effects() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLaserPresentation::spawn_hit_effects);

    static FName const colour_parameter{TEXT("User.Colour")};
    static FName const ribbon_colour_parameter{TEXT("User.Ribbon_Colour")};

    auto* const hit_effect{actor_config->hit_effect.Get()};
    if (!IsValid(hit_effect)) {
        if (!have_warned_hit_effect) {
            UE_LOG(LogSandbox,
                   Warning,
                   TEXT("FLaserPresentation::spawn_hit_effects: hit_effect is nullptr"));
            have_warned_hit_effect = true;
        }
        return;
    }

    auto const& hit_details{simulation().hit_details};
    auto const n{ml::num(hit_details)};
    if (n < 1) {
        return;
    }

    auto* const world{instances->GetWorld()};
    for (int32 i{0}; i < n; ++i) {
        constexpr bool auto_destroy{true};
        constexpr bool auto_activate{false};
        auto* const system{UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            world,
            hit_effect,
            ml::get_vector3d(hit_details.locations, i),
            FRotator::ZeroRotator,
            FVector::OneVector,
            auto_destroy,
            auto_activate,
            ENCPoolMethod::AutoRelease)};
        if (!IsValid(system)) {
            continue;
        }

        constexpr double colour_scale{20.0};
        auto const colour{hit_details.colours[i] * colour_scale};
        system->SetVariableLinearColor(colour_parameter, colour);
        system->SetVariableLinearColor(ribbon_colour_parameter, colour);
        system->Activate();
    }
}

void FLaserPresentation::validate_array_sizes() const {
    simulation().validate_array_sizes();
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(simulation().get_num_instances()),
        SANDBOX_NAMED_NUM(material_data.Num()),
        SANDBOX_NAMED_NUM(instances->get_instance_count()),
    });
}
