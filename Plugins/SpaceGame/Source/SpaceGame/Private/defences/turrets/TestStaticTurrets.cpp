#include "SpaceGame/defences/turrets/TestStaticTurrets.h"

#include <SandboxGameShared/utilities/actor_utils.h>
#include <SpaceGame/entities/TestBatchActorCore.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestTeamVisualData.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <Engine/StaticMesh.h>
#include <NiagaraFunctionLibrary.h>
#include <NiagaraSystem.h>

ATestStaticTurrets::ATestStaticTurrets()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    RootComponent->SetMobility(EComponentMobility::Static);

    instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    ml::set_actor_component_mobility(*this, EComponentMobility::Static);
}

void ATestStaticTurrets::set_actor_config(FTurretConfig const* const new_config) noexcept {
    actor_config = new_config;
}

void ATestStaticTurrets::bind_simulation(ml::test_static_turrets::Simulation& new_simulation) {
    bound_simulation = &new_simulation;
    bound_simulation->search_slice_size = search_slice_size;
}

auto ATestStaticTurrets::simulation() -> ml::test_static_turrets::Simulation& {
    check(bound_simulation);
    return *bound_simulation;
}

auto ATestStaticTurrets::simulation() const -> ml::test_static_turrets::Simulation const& {
    return const_cast<ATestStaticTurrets*>(this)->simulation();
}

void ATestStaticTurrets::clear_runtime_state_presentation() {
    instances->ClearInstances();
    ismc_transforms.Reset();
}

void ATestStaticTurrets::begin_play_presentation(TArray<FTransform> initial_transforms) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::begin_play_presentation);
    if (!actor_config) {
        UE_LOG(LogSandbox, Fatal, TEXT("ATestStaticTurrets actor_config is nullptr."));
    }
    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(instances.Get()),
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->mesh.Get()),
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->team_visual_data.Get()),
    });
    debug_drawer = actor_config->debug_drawer;
    debug_drawer.world = GetWorld();

    configure_ismc();
    ismc_transforms = MoveTemp(initial_transforms);
    add_initial_visual_instances();
    validate_array_sizes();
}

void ATestStaticTurrets::update_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::update_visual_data);
    auto const& indices_to_remove{simulation().presentation_indices_to_remove};
    if (!indices_to_remove.IsEmpty()) {
        trigger_death_effects();
        ml::remove_at_swap_many_sorted_desc(indices_to_remove, ismc_transforms);
        constexpr bool reverse_sorted{true};
        instances->RemoveInstances(indices_to_remove, reverse_sorted);

        constexpr bool mark_render_state_dirty{false};
        constexpr bool teleport{true};
        instances->BatchUpdateInstancesTransforms(
            0, ismc_transforms, is_world_space, mark_render_state_dirty, teleport);
    }
}

void ATestStaticTurrets::commit_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::commit_visual_data);

    instances->MarkRenderStateDirty();
    if (draw_target_arrows_enabled || draw_debug_entity_info_enabled) {
        draw_debugging_shapes();
    }
}

void ATestStaticTurrets::end_tick_presentation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::end_tick_presentation);
    validate_array_sizes();
}

void ATestStaticTurrets::configure_ismc() {
    RootComponent->SetMobility(EComponentMobility::Static);
    ml::batch::configure_ismc(*instances,
                              {
                                  .mesh = actor_config->mesh.Get(),
                                  .num_custom_data_floats = n_custom_ismc_floats,
                              });
}

void ATestStaticTurrets::add_initial_visual_instances() {
    auto const& entities{simulation().entities};
    auto const n_to_add{entities.num()};
    if (n_to_add == 0) {
        return;
    }

    auto const colour_cache{
        UTestTeamVisualData::build_team_colour_cache(actor_config->team_visual_data)};
    TArray<float> custom_data;
    custom_data.SetNumUninitialized(n_to_add * n_custom_ismc_floats, EAllowShrinking::No);
    for (int32 i{0}; i < n_to_add; ++i) {
        auto const base{i * n_custom_ismc_floats};
        auto const& colour{colour_cache[entities.teams[i]]};
        custom_data[base + 0] = colour.R;
        custom_data[base + 1] = colour.G;
        custom_data[base + 2] = colour.B;
    }

    instances->AddInstances(ismc_transforms, false);
    instances->SetCustomData(0, n_to_add - 1, custom_data, false);
    validate_array_sizes();
}

void ATestStaticTurrets::trigger_death_effects() {
    auto const& death_locations{simulation().presentation_death_locations};
    auto const n{death_locations.Num()};
    auto* world{GetWorld()};
    auto* explosion_system{actor_config->death_effect.Get()};

    if (!IsValid(explosion_system)) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("ATestStaticTurrets::trigger_death_effects: death_effect is nullptr"));
        return;
    }

    FVector const scale{actor_config->death_effect_scale};
    FVector const location_offset{actor_config->death_effect_offset};

    for (int32 i{0}; i < n; ++i) {

        constexpr bool auto_destroy{true};
        constexpr bool auto_activate{true};

        UNiagaraFunctionLibrary::SpawnSystemAtLocation(world,
                                                       explosion_system,
                                                       FVector{death_locations[i]} +
                                                           location_offset,
                                                       FRotator::ZeroRotator,
                                                       scale,
                                                       auto_destroy,
                                                       auto_activate,
                                                       ENCPoolMethod::AutoRelease);
    }
}

void ATestStaticTurrets::validate_array_sizes() const {
    simulation().validate_array_sizes();
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(simulation().get_num_instances()),
        SANDBOX_NAMED_NUM(ismc_transforms),
        SANDBOX_NAMED_NUM(instances->GetNumInstances()),
    });
}

void ATestStaticTurrets::draw_debugging_shapes() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestStaticTurrets::draw_debugging_shapes);

    auto const& turret_simulation{simulation()};
    auto const& entities{turret_simulation.entities};
    auto const* const entity_registry{turret_simulation.entity_registry};
    check(entity_registry);
    auto const n{turret_simulation.get_num_instances()};
    auto const text_offset{actor_config->debug_status_text_offset};

    auto& drawer{debug_drawer};
    for (int32 i{0}; i < n; ++i) {
        auto const turret_location{ml::get_vector3d(entities.locations, i)};

        if (draw_target_arrows_enabled) {
            auto const target_handle{entities.target_handles[i]};

            if (entity_registry->is_valid_handle(target_handle)) {
                auto const target_location{ml::get_vector3d(entities.target_locations, i)};
                drawer.draw_line(turret_location, target_location);
            }
        }

        if (draw_debug_entity_info_enabled) {
            auto const turret_handle{entities.handles[i]};

            auto const msg{FString::Printf(TEXT("[%d, %d] HP=%d"),
                                           turret_handle.index,
                                           turret_handle.generation,
                                           entities.healths[i])};
            auto const msg_location{turret_location + text_offset};
            drawer.draw_string(msg_location, msg);
        }
    }
}
