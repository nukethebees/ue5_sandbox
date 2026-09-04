#include "SpaceGame/combat/lasers/TestLasers.h"

#include <SandboxGameShared/utilities/actor_utils.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

#include <Async/ParallelFor.h>
#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <NiagaraFunctionLibrary.h>
#include <ProfilingDebugging/CountersTrace.h>

TRACE_DECLARE_INT_COUNTER(SandboxTestLaserISMCCount, TEXT("Sandbox/TestLaserISMCCount"));

ATestLasers::ATestLasers()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));
    instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    ml::set_actor_component_mobility(*this, EComponentMobility::Static);
}

void ATestLasers::bind_simulation(ml::test_lasers::Simulation& new_simulation) {
    bound_simulation = &new_simulation;
}

auto ATestLasers::simulation() -> ml::test_lasers::Simulation& {
    check(bound_simulation);
    return *bound_simulation;
}

auto ATestLasers::simulation() const -> ml::test_lasers::Simulation const& {
    return const_cast<ATestLasers*>(this)->simulation();
}

void ATestLasers::clear_runtime_state_presentation() {
    instances->ClearInstances();
    ismc_data.Reset();
    dummy_transforms_spawn_buffer.Reset();
}

void ATestLasers::begin_play_presentation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::begin_play_presentation);
    TRACE_COUNTER_SET(SandboxTestLaserISMCCount, 0);

    if (!actor_config) {
        UE_LOG(LogSandboxLearning, Fatal, TEXT("actor_config is nullptr."));
    }
    if (!actor_config->mesh || !actor_config->material) {
        UE_LOG(LogSandboxLearning, Fatal, TEXT("actor_config is not ready."));
    }

    configure_ismc();
    instances->PreAllocateInstancesMemory(actor_config->n_preallocated_instances);

#if WITH_EDITOR
    debug_drawer.world = GetWorld();
#endif

    validate_array_sizes();
}

void ATestLasers::update_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::update_visual_data);
    apply_simulation_changes_to_ismc();
    prepare_ismc_transforms();
    update_ismc();
}

void ATestLasers::commit_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::commit_visual_data);
    instances->MarkRenderStateDirty();
    spawn_hit_effects();
}

void ATestLasers::end_tick_presentation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::end_tick_presentation);
    TRACE_COUNTER_SET(SandboxTestLaserISMCCount, instances->GetNumInstances());
    validate_array_sizes();
}

void ATestLasers::configure_ismc() {
    instances->SetMobility(EComponentMobility::Movable);
    check(instances->SetStaticMesh(actor_config->mesh));
    instances->SetMobility(EComponentMobility::Static);
    instances->SetMaterial(0, actor_config->material);

    instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    instances->SetGenerateOverlapEvents(false);
    instances->SetCanEverAffectNavigation(false);
    instances->SetCastShadow(false);
    instances->SetAffectDistanceFieldLighting(false);
    instances->SetReceivesDecals(false);
    instances->SetCullDistances(actor_config->min_cull_distance, actor_config->max_cull_distance);
    instances->SetNumCustomDataFloats(n_custom_ismc_floats);
    instances->SetRemoveSwap();
}

void ATestLasers::apply_simulation_changes_to_ismc() {
    auto const& laser_simulation{simulation()};
    for (auto const index : laser_simulation.presentation_indices_to_remove) {
        instances->RemoveInstance(index);
    }

    auto const spawn_count{laser_simulation.presentation_spawn_count};
    if (spawn_count > 0) {
        auto const offset{instances->GetNumInstances()};
        dummy_transforms_spawn_buffer.SetNum(spawn_count, EAllowShrinking::No);

        constexpr bool return_indices{false};
        constexpr bool update_navigation{false};
        instances->AddInstances(
            dummy_transforms_spawn_buffer, return_indices, is_world_space, update_navigation);
        instances->SetCustomData(offset,
                                 offset + spawn_count - 1,
                                 laser_simulation.presentation_custom_data_to_add,
                                 false);
    }

    check(instances->GetNumInstances() == laser_simulation.get_num_instances());
}

void ATestLasers::prepare_ismc_transforms() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::prepare_ismc_transforms);

    auto const& laser_simulation{simulation()};
    auto const n{laser_simulation.get_num_instances()};
    ismc_data.SetNumUninitialized(n, EAllowShrinking::No);
    if (n <= 0) {
        return;
    }

    constexpr int32 n_jobs{8};
    auto const updates_per_slice{FMath::DivideAndRoundUp(n, n_jobs)};
    ParallelFor(n_jobs, [this, &laser_simulation, updates_per_slice, n](int32 const job_index) {
        auto const begin{job_index * updates_per_slice};
        auto const end{FMath::Min(begin + updates_per_slice, n)};
        for (int32 i{begin}; i < end; ++i) {
            auto const rotation{ml::get_rotator3d(laser_simulation.entities.rotations, i)};
            ismc_data[i] =
                FTransform{
                    rotation.Quaternion(),
                    ml::get_vector3d(laser_simulation.entities.locations, i),
                }
                    .ToMatrixWithScale();
        }
    });
}

void ATestLasers::update_ismc() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::update_ismc);

    constexpr bool mark_render_dirty{false};
    constexpr bool teleport{true};
    instances->BatchUpdateInstancesData(
        0, ismc_data.Num(), ismc_data.GetData(), mark_render_dirty, teleport);
}

void ATestLasers::spawn_hit_effects() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestLasers::spawn_hit_effects);

    static FName const colour_parameter{TEXT("User.Colour")};
    static FName const ribbon_colour_parameter{TEXT("User.Ribbon_Colour")};

    auto* const hit_effect{actor_config->hit_effect.Get()};
    if (!IsValid(hit_effect)) {
        if (!have_warned_hit_effect) {
            UE_LOG(
                LogSandbox, Warning, TEXT("ATestLasers::spawn_hit_effects: hit_effect is nullptr"));
            have_warned_hit_effect = true;
        }
        return;
    }

    auto const& hit_details{simulation().hit_details};
    auto const n{ml::num(hit_details)};
    if (n < 1) {
        return;
    }

    auto* const world{GetWorld()};
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

void ATestLasers::validate_array_sizes() const {
    simulation().validate_array_sizes();
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(simulation().get_num_instances()),
        SANDBOX_NAMED_NUM(instances->GetNumInstances()),
    });
}
