#include "SpaceGame/ships/fighters/TestCapitalShipFighters.h"

#include <SandboxGameShared/utilities/actor_utils.h>
#include <SpaceGame/entities/TestBatchActorCore.h>
#include <SpaceGame/entities/TestTeamVisualData.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>
#include <SpaceGame/support/logging/SandboxVisualLoggerStyle.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Async/ParallelFor.h>
#include <Components/SceneComponent.h>
#include <Engine/StaticMesh.h>
#include <VisualLogger/VisualLogger.h>

namespace {
auto get_visual_logger_entity_colour(FSandboxVisualLoggerEntityStyle const& style,
                                     ETestTeam const team) -> FColor {
    switch (team) {
        case ETestTeam::Blue:
            return style.friendly_entity_colour;
        case ETestTeam::Red:
            return style.enemy_entity_colour;
        default:
            return style.neutral_entity_colour;
    }
}
} // namespace

ATestCapitalShipFighters::ATestCapitalShipFighters()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));
    instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    ml::set_actor_component_mobility(*this, EComponentMobility::Static);
}

void ATestCapitalShipFighters::set_actor_config(FFighterConfig const* const new_config) noexcept {
    actor_config = new_config;
}

void ATestCapitalShipFighters::bind_simulation(
    ml::test_capital_ship_fighters::Simulation& new_simulation) {
    bound_simulation = &new_simulation;
}

auto ATestCapitalShipFighters::simulation() -> ml::test_capital_ship_fighters::Simulation& {
    check(bound_simulation);
    return *bound_simulation;
}

auto ATestCapitalShipFighters::simulation() const
    -> ml::test_capital_ship_fighters::Simulation const& {
    return const_cast<ATestCapitalShipFighters*>(this)->simulation();
}

void ATestCapitalShipFighters::clear_runtime_state_presentation() {
    instances->ClearInstances();
    ismc_transforms.Reset();
    dummy_transforms_spawn_buffer.Reset();
    custom_data_buffer.Reset();
}

void ATestCapitalShipFighters::begin_play_presentation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::begin_play_presentation);

    if (!actor_config) {
        UE_LOG(LogSandbox, Fatal, TEXT("ATestCapitalShipFighters actor_config is nullptr."));
    }
    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->mesh.Get()),
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->team_visual_data.Get()),
    });

    configure_ismc();

    debug_drawer = actor_config->debug_drawer;
    debug_drawer.world = GetWorld();
}

void ATestCapitalShipFighters::update_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::update_visual_data);
    apply_simulation_changes_to_ismc();
    prepare_ismc_transforms();
    update_ismc();
}

void ATestCapitalShipFighters::commit_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::commit_visual_data);
    instances->MarkRenderStateDirty();
    if (enable_target_debug_drawing || enable_ship_location_debug_drawing) {
        draw_debug_shapes();
    }
}

void ATestCapitalShipFighters::end_tick_presentation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::end_tick_presentation);
    validate_array_sizes();
    visual_log_state();
}

void ATestCapitalShipFighters::configure_ismc() {
    ml::batch::configure_ismc(*instances,
                              {
                                  .mesh = actor_config->mesh.Get(),
                                  .num_custom_data_floats = n_custom_ismc_floats,
                              });
}

void ATestCapitalShipFighters::apply_simulation_changes_to_ismc() {
    auto const& fighter_simulation{simulation()};
    auto const& indices_to_remove{fighter_simulation.presentation_indices_to_remove};
    if (!indices_to_remove.IsEmpty()) {
        ml::remove_at_swap_many_sorted_desc(indices_to_remove, ismc_transforms);
        constexpr bool reverse_sorted{true};
        instances->RemoveInstances(indices_to_remove, reverse_sorted);
    }

    auto const spawn_count{fighter_simulation.presentation_spawn_count};
    if (spawn_count > 0) {
        ismc_transforms.AddDefaulted(spawn_count);
        dummy_transforms_spawn_buffer.SetNum(spawn_count, EAllowShrinking::No);
        constexpr bool return_indices{false};
        constexpr bool update_navigation{false};
        instances->AddInstances(
            dummy_transforms_spawn_buffer, return_indices, is_world_space, update_navigation);
    }

    check(instances->GetNumInstances() == fighter_simulation.get_num_instances());
    write_ismc_custom_data(0, fighter_simulation.get_num_instances());
}

void ATestCapitalShipFighters::prepare_ismc_transforms() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::prepare_ismc_transforms);

    auto const& data{simulation().entity_buffers.current()};
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

void ATestCapitalShipFighters::update_ismc() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::update_ismc);
    constexpr bool mark_render_state_dirty{false};
    constexpr bool teleport{true};
    instances->BatchUpdateInstancesTransforms(
        0, ismc_transforms, is_world_space, mark_render_state_dirty, teleport);
}

void ATestCapitalShipFighters::draw_debug_shapes() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::draw_debug_shapes);

    auto const& data{simulation().entity_buffers.current()};
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

void ATestCapitalShipFighters::write_ismc_custom_data(int32 const offset, int32 const count) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::write_ismc_custom_data);
    check(count >= 0);
    if (count == 0) {
        return;
    }

    auto const& data{simulation().entity_buffers.current()};
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

void ATestCapitalShipFighters::visual_log_state() const {
#if ENABLE_VISUAL_LOG
    if (!FVisualLogger::IsRecording()) {
        return;
    }
#else
    return;
#endif

    auto const& fighter_simulation{simulation()};
    auto const* const entity_registry{fighter_simulation.entity_registry};
    if (!entity_registry) {
        UE_LOG(LogSandboxEntities,
               Error,
               TEXT("ATestCapitalShipFighters::visual_log_state entity registry is null"));
        return;
    }
    if (!actor_config) {
        UE_LOG(LogSandboxEntities,
               Error,
               TEXT("ATestCapitalShipFighters::visual_log_state actor_config is nullptr"));
        return;
    }
    if (auto const message{ml::report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(actor_config->visual_logger_style),
        })}) {
        UE_LOG(LogSandboxEntities,
               Error,
               TEXT("ATestCapitalShipFighters::visual_log_state UObject ptrs are invalid:\n%s"),
               *message);
        return;
    }

    auto const& style{*actor_config->visual_logger_style};
    auto const normal_line_thickness{static_cast<uint16>(style.lines.normal_line_thickness)};
    auto const highlighted_line_thickness{
        static_cast<uint16>(style.lines.highlighted_line_thickness)};
    auto const& data{fighter_simulation.entity_buffers.current()};
    auto const fighter_count{data.num()};
    for (int32 fighter_index{0}; fighter_index < fighter_count; ++fighter_index) {
        auto const fighter_handle{data.entity_handles[fighter_index]};
        FVector const fighter_location{ml::get_vector3f(data.locations, fighter_index)};
        auto const fighter_colour{
            get_visual_logger_entity_colour(style.entities, data.teams[fighter_index])};
        UE_VLOG_SPHERE(this,
                       LogSandboxEntities,
                       Log,
                       fighter_location,
                       style.entities.fighter_entity_radius,
                       fighter_colour,
                       TEXT("Fighter %d"),
                       fighter_handle.index);

        FVector const desired_move_location{
            ml::get_vector3f(data.desired_move_locations, fighter_index)};
        UE_VLOG_WIRESPHERE(this,
                           LogSandboxNavigation,
                           Log,
                           desired_move_location,
                           style.entities.fighter_entity_radius,
                           style.navigation.movement_destination_colour,
                           TEXT("Move destination"));
        UE_VLOG_SEGMENT_THICK(this,
                              LogSandboxNavigation,
                              Log,
                              fighter_location,
                              desired_move_location,
                              style.navigation.movement_destination_colour,
                              normal_line_thickness,
                              TEXT("Move destination"));

        auto const target_handle{data.target_handles[fighter_index]};
        if (!entity_registry->is_valid_alive(target_handle)) {
            continue;
        }
        FVector const target_location{ml::get_vector3f(data.target_locations, fighter_index)};
        UE_VLOG_WIRESPHERE(this,
                           LogSandboxTargeting,
                           Log,
                           target_location,
                           style.entities.fighter_entity_radius,
                           style.combat.selected_target_colour,
                           TEXT("Target %d"),
                           target_handle.index);
        UE_VLOG_SEGMENT_THICK(this,
                              LogSandboxTargeting,
                              Log,
                              fighter_location,
                              target_location,
                              style.combat.selected_target_colour,
                              highlighted_line_thickness,
                              TEXT("Target %d"),
                              target_handle.index);
    }
}

void ATestCapitalShipFighters::validate_array_sizes() const {
    simulation().validate_array_sizes();
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(simulation().get_num_instances()),
        SANDBOX_NAMED_NUM(ismc_transforms),
        SANDBOX_NAMED_NUM(instances->GetNumInstances()),
    });
}
