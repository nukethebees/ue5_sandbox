#include "SpaceGame/presentation/CapitalPresentation.h"

#include <SandboxGameShared/utilities/actor_utils.h>
#include <SpaceGame/entities/TestBatchActorCore.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestTeamVisualData.h>
#include <SpaceGame/presentation/DelayedNiagaraSpawns.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>
#include <SpaceGame/support/logging/SandboxVisualLoggerStyle.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/transforms.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <Engine/StaticMesh.h>
#include <NiagaraSystem.h>
#include <VisualLogger/VisualLogger.h>

FCapitalPresentation::FCapitalPresentation(UInstancedStaticMeshComponent& component)
    : instances{&component} {}

void FCapitalPresentation::set_actor_config(FCapitalShipConfig const* const new_config) noexcept {
    actor_config = new_config;
}

void FCapitalPresentation::bind_simulation(ml::test_capital_ships::Simulation& new_simulation) {
    bound_simulation = &new_simulation;
}

auto FCapitalPresentation::simulation() -> ml::test_capital_ships::Simulation& {
    check(bound_simulation);
    return *bound_simulation;
}

auto FCapitalPresentation::simulation() const -> ml::test_capital_ships::Simulation const& {
    return const_cast<FCapitalPresentation*>(this)->simulation();
}

void FCapitalPresentation::set_niagara_spawner(FDelayedNiagaraSpawns& spawner) {
    niagara_spawner = &spawner;
}

void FCapitalPresentation::clear_runtime_state_presentation() {
    instances->ClearInstances();
}

void FCapitalPresentation::begin_play_presentation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCapitalPresentation::begin_play_presentation);

    auto* const world{instances->GetWorld()};
    if (!actor_config) {
        UE_LOG(LogSandbox, Fatal, TEXT("FCapitalPresentation actor_config is nullptr."));
    }
    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(world),
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->mesh.Get()),
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->team_visual_data.Get()),
    });

    debug_drawer = actor_config->debug_drawer;
    debug_drawer.world = world;

    configure_ismc();
    add_initial_visual_instances();
    validate_array_sizes();
}

void FCapitalPresentation::update_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCapitalPresentation::update_visual_data);

    if (simulation().presentation_spawn_count > 0) {
        add_visual_instances(simulation().presentation_spawn_start,
                             simulation().presentation_spawn_count);
    }

    auto const& indices_to_remove{simulation().presentation_indices_to_remove};
    if (!indices_to_remove.IsEmpty()) {
        trigger_death_effects();
        constexpr bool reverse_sorted{true};
        instances->RemoveInstances(indices_to_remove, reverse_sorted);
    }
}

void FCapitalPresentation::commit_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCapitalPresentation::commit_visual_data);

    instances->MarkRenderStateDirty();
    if (debugging_shapes_enabled) {
        draw_debugging_shapes();
    }
}

void FCapitalPresentation::end_tick_presentation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCapitalPresentation::end_tick_presentation);
    validate_array_sizes();
    visual_log_state();
}

void FCapitalPresentation::configure_ismc() {
    ml::batch::configure_ismc(*instances,
                              {
                                  .mesh = actor_config->mesh.Get(),
                                  .material = actor_config->material.Get(),
                                  .num_custom_data_floats = n_custom_ismc_floats,
                              });
}

void FCapitalPresentation::add_initial_visual_instances() {
    auto const& entities{simulation().entities};
    auto const n_to_add{entities.num()};
    add_visual_instances(0, n_to_add);
}

void FCapitalPresentation::add_visual_instances(int32 const first_index, int32 const n_to_add) {
    auto const& entities{simulation().entities};
    if (n_to_add == 0) {
        return;
    }

    auto const colour_cache{
        UTestTeamVisualData::build_team_colour_cache(actor_config->team_visual_data)};
    TArray<float> custom_data;
    custom_data.SetNumUninitialized(n_to_add * n_custom_ismc_floats, EAllowShrinking::No);
    for (int32 i{0}; i < n_to_add; ++i) {
        auto const base{i * n_custom_ismc_floats};
        auto const& colour{colour_cache[entities.teams[first_index + i]]};
        custom_data[base + 0] = colour.R;
        custom_data[base + 1] = colour.G;
        custom_data[base + 2] = colour.B;
    }

    auto const transforms{
        ml::make_transforms(entities.locations.get_const_view(first_index, n_to_add),
                            entities.rotations.get_const_view(first_index, n_to_add))};
    constexpr bool return_indices{false};
    constexpr bool update_navigation{false};
    instances->AddInstances(transforms, return_indices, is_world_space, update_navigation);
    constexpr bool mark_render_state_dirty{false};
    instances->SetCustomData(
        first_index, first_index + n_to_add - 1, custom_data, mark_render_state_dirty);
}

void FCapitalPresentation::trigger_death_effects() {
    auto const& death_locations{simulation().presentation_death_locations};
    auto const n{death_locations.Num()};
    auto* const small_death_explosion{actor_config->small_death_explosion.Get()};
    auto* const main_death_explosion{actor_config->main_death_explosion.Get()};

    if (!IsValid(small_death_explosion)) {
        UE_LOG(
            LogSandbox,
            Warning,
            TEXT("FCapitalPresentation::trigger_death_effects: small_death_explosion is nullptr"));
        return;
    }
    if (!IsValid(main_death_explosion)) {
        UE_LOG(
            LogSandbox,
            Warning,
            TEXT("FCapitalPresentation::trigger_death_effects: main_death_explosion is nullptr"));
        return;
    }
    if (!niagara_spawner) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("FCapitalPresentation::trigger_death_effects: niagara_spawner is nullptr"));
        return;
    }

    auto const n_small_explosions{actor_config->n_small_explosions};
    auto const time_between_explosions{actor_config->time_between_explosions};
    auto const min_range{actor_config->min_small_explosion_range};
    auto const max_range{actor_config->max_small_explosion_range};
    auto main_explosion_delay{actor_config->large_explosion_delay};
    if (actor_config->main_explosion_delay_mode ==
            ECapitalShipMainExplosionDelayMode::AfterSmallExplosions &&
        n_small_explosions > 1) {
        main_explosion_delay += n_small_explosions * (n_small_explosions - 1);
    }

    TArray<UNiagaraSystem*> spawn_systems;
    TArray<FVector> spawn_locations;
    TArray<float> spawn_delays;
    ml::reserve(n * (n_small_explosions + 1), spawn_systems, spawn_locations, spawn_delays);

    float current_delay{0.f};
    for (auto const& death_location : death_locations) {
        FVector const base_location{death_location};
        for (int32 explosion_index{0}; explosion_index < n_small_explosions; ++explosion_index) {
            if (explosion_index > 0) {
                current_delay += time_between_explosions;
            }
            auto const offset{FVector{
                FMath::FRandRange(min_range.X, max_range.X),
                FMath::FRandRange(min_range.Y, max_range.Y),
                FMath::FRandRange(min_range.Z, max_range.Z),
            }};
            spawn_systems.Add(small_death_explosion);
            spawn_locations.Add(base_location + offset);
            spawn_delays.Add(current_delay);
        }

        spawn_systems.Add(main_death_explosion);
        spawn_locations.Add(base_location);
        spawn_delays.Add(main_explosion_delay);
    }

    niagara_spawner->add_spawns(spawn_systems, spawn_locations, spawn_delays);
}

void FCapitalPresentation::draw_debugging_shapes() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCapitalPresentation::draw_debugging_shapes);

    auto const& capital_simulation{simulation()};
    auto const& entities{capital_simulation.entities};
    auto const* const entity_registry{capital_simulation.entity_registry};
    check(entity_registry);
    auto const n{capital_simulation.get_num_instances()};
    auto const text_offset{actor_config->debug_status_text_offset};
    for (int32 i{0}; i < n; ++i) {
        auto const ship_location{ml::get_vector3d(entities.locations, i)};
        auto const target_handle{entities.target_handles[i]};
        if (entity_registry->is_valid_handle(target_handle)) {
            FVector3d const target_location{entity_registry->get_location(target_handle)};
            debug_drawer.draw_arrow(ship_location, target_location);
        }

        auto const ship_handle{entities.handles[i]};
        auto const message{FString::Printf(TEXT("[%d, %d] HP=%d"),
                                           ship_handle.index,
                                           ship_handle.generation,
                                           entities.healths[i])};
        debug_drawer.draw_string(ship_location + text_offset, message);
    }
}

void FCapitalPresentation::visual_log_state() const {
#if ENABLE_VISUAL_LOG
    if (!FVisualLogger::IsRecording()) {
        return;
    }
#else
    return;
#endif

    auto const& capital_simulation{simulation()};
    auto const* const entity_registry{capital_simulation.entity_registry};
    if (!entity_registry) {
        UE_LOG(LogSandboxEntities,
               Error,
               TEXT("FCapitalPresentation::visual_log_state entity registry is null"));
        return;
    }
    if (!actor_config) {
        UE_LOG(LogSandboxEntities,
               Error,
               TEXT("FCapitalPresentation::visual_log_state actor_config is nullptr"));
        return;
    }
    if (auto const message{ml::report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(actor_config->visual_logger_style),
        })}) {
        UE_LOG(LogSandboxEntities,
               Error,
               TEXT("FCapitalPresentation::visual_log_state UObject ptrs are invalid:\n%s"),
               *message);
        return;
    }

    auto const& entities{capital_simulation.entities};
    auto const& style{*actor_config->visual_logger_style};
    FVector const capital_extent{style.entities.capital_ship_box_extent};
    auto const normal_line_thickness{static_cast<uint16>(style.lines.normal_line_thickness)};
    auto const highlighted_line_thickness{
        static_cast<uint16>(style.lines.highlighted_line_thickness)};
    auto const capital_count{capital_simulation.get_num_instances()};
    for (int32 capital_index{0}; capital_index < capital_count; ++capital_index) {
        FVector const capital_location{ml::get_vector3f(entities.locations, capital_index)};
        FBox const capital_box{capital_location - capital_extent,
                               capital_location + capital_extent};
        UE_VLOG_BOX(instances->GetOwner(),
                    LogSandboxEntities,
                    Log,
                    capital_box,
                    style.entities.capital_ship_colour,
                    TEXT("Capital %d Team %d"),
                    entities.handles[capital_index].index,
                    static_cast<int32>(entities.teams[capital_index]));

        auto const fighter_handles{capital_simulation.get_fighter_handles(capital_index)};
        auto const fighter_count{fighter_handles.Num()};
        for (int32 fighter_index{0}; fighter_index < fighter_count; ++fighter_index) {
            auto const fighter_handle{fighter_handles[fighter_index]};
            if (!entity_registry->is_valid_alive(fighter_handle)) {
                continue;
            }
            FVector const fighter_location{entity_registry->get_location(fighter_handle)};
            UE_VLOG_SEGMENT_THICK(instances->GetOwner(),
                                  LogSandboxEntities,
                                  Log,
                                  capital_location,
                                  fighter_location,
                                  style.navigation.parent_to_child_line_colour,
                                  normal_line_thickness,
                                  TEXT("Fighter %d"),
                                  fighter_handle.index);
        }

        auto const target_handle{entities.target_handles[capital_index]};
        if (!entity_registry->is_valid_alive(target_handle)) {
            continue;
        }
        FVector const target_location{entity_registry->get_location(target_handle)};
        UE_VLOG_WIRESPHERE(instances->GetOwner(),
                           LogSandboxTargeting,
                           Log,
                           target_location,
                           style.entities.fighter_entity_radius,
                           style.combat.selected_target_colour,
                           TEXT("Target %d"),
                           target_handle.index);
        UE_VLOG_SEGMENT_THICK(instances->GetOwner(),
                              LogSandboxTargeting,
                              Log,
                              capital_location,
                              target_location,
                              style.combat.selected_target_colour,
                              highlighted_line_thickness,
                              TEXT("Target %d"),
                              target_handle.index);
    }
}

void FCapitalPresentation::validate_array_sizes() const {
    simulation().validate_array_sizes();
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(simulation().get_num_instances()),
        SANDBOX_NAMED_NUM(instances->GetNumInstances()),
    });
}
