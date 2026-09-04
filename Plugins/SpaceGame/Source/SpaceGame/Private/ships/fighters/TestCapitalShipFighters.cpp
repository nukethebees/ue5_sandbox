#include "SpaceGame/ships/fighters/TestCapitalShipFighters.h"

#include <SandboxGameShared/utilities/actor_utils.h>
#include <SpaceGame/combat/lasers/TestLasers.h>
#include <SpaceGame/entities/DirectDamageEvents.h>
#include <SpaceGame/entities/TestBatchActorCore.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestTeamVisualData.h>
#include <SpaceGame/simulation/SpatialQueryManager.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>
#include <SpaceGame/support/logging/SandboxVisualLoggerStyle.h>
#include <SpaceGame/support/mesh.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/projectile_intercept.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/transforms.h>
#include <SandboxCoreEngine/uobject_utils.h>
#include <SandboxNative/deterministic_bias.h>

#include <Async/ParallelFor.h>
#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <Engine/StaticMesh.h>
#include <Engine/World.h>
#include <Misc/Optional.h>
#include <ProfilingDebugging/CountersTrace.h>
#include <Templates/Greater.h>
#include <VisualLogger/VisualLogger.h>

#include <array>

TRACE_DECLARE_INT_COUNTER(SandboxTestFighterCount, TEXT("Sandbox/TestFighterCount"));

namespace {
inline auto get_span(FVectors3f& vec, FIndexSpan const span) {
    return vec.get_view().slice(span.offset, span.count);
}
inline auto get_span(FVectors3f const& vec, FIndexSpan const span) {
    return vec.get_view().slice(span.offset, span.count);
}

auto find_appropriate_fire_point(ml::FSpatialQueryManager const& spatial_query_manager,
                                 FVector3f const target_location,
                                 FVector3f const reference_location,
                                 float const fire_point_distance,
                                 float const trace_end_offset,
                                 float const desired_attack_distance,
                                 uint32 const integral_bias,
                                 float const float_bias) -> TOptional<FVector3f> {
    struct Offset {
        float X;
        float Y;
    };
    static constexpr std::array<Offset, 16> fire_point_angle_offsets{{
        {0.f, 0.f},

        {45.f, 0.f},
        {-45.f, 0.f},
        {90.f, 0.f},
        {-90.f, 0.f},
        {135.f, 0.f},
        {-135.f, 0.f},
        {180.f, 0.f},

        {0.f, 35.f},
        {90.f, 35.f},
        {180.f, 35.f},
        {-90.f, 35.f},

        {45.f, -35.f},
        {135.f, -35.f},
        {-135.f, -35.f},
        {-45.f, -35.f},
    }};

    auto const base_direction{(reference_location - target_location).GetSafeNormal()};
    auto const base_candidate_rotation{base_direction.ToOrientationRotator()};
    auto const pattern_yaw_offset{float_bias * 360.f};
    auto const n_offsets{static_cast<uint32>(fire_point_angle_offsets.size())};
    auto const first_offset_index{integral_bias % n_offsets};
    FVector3d const trace_target{target_location};

    for (uint32 offset{}; offset < n_offsets; ++offset) {
        auto const offset_index{(first_offset_index + offset) % n_offsets};
        auto const angle_offset{fire_point_angle_offsets[offset_index]};

        auto candidate_rotation{base_candidate_rotation};
        candidate_rotation.Yaw += pattern_yaw_offset + angle_offset.X;
        candidate_rotation.Pitch += angle_offset.Y;

        auto const candidate_direction{candidate_rotation.Vector()};
        auto const candidate_location{target_location +
                                      candidate_direction * desired_attack_distance};

        auto const candidate_aim_direction{
            (trace_target - FVector3d{candidate_location}).GetSafeNormal()};
        auto const trace_start{FVector3d{candidate_location} +
                               candidate_aim_direction * fire_point_distance};
        auto const trace_direction{(trace_target - trace_start).GetSafeNormal()};
        auto const trace_end{trace_target - trace_direction * trace_end_offset};

        if (spatial_query_manager.has_clear_line(FVector3f{trace_start}, FVector3f{trace_end})) {
            return candidate_location;
        }
    }

    return NullOpt;
}

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
}

ATestCapitalShipFighters::ATestCapitalShipFighters()
    : instances{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("instances"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    ml::set_actor_component_mobility(*this, EComponentMobility::Static);
}

void ATestCapitalShipFighters::bind_simulation_clock(
    ATestBatchOrchestrator const& orchestrator) noexcept {
    simulation_clock.bind(orchestrator);
}

// Actor life cycle
void ATestCapitalShipFighters::begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::begin_play);
    TRACE_COUNTER_SET(SandboxTestFighterCount, 0);
    check(entity_registry);
    check(spatial_query_manager);
    if (!actor_config) {
        UE_LOG(LogSandbox, Fatal, TEXT("ATestCapitalShipFighters actor_config is nullptr."));
    }

    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->mesh),
        SANDBOX_NAMED_UOBJECT_PTR(laser_actor),
    });

    ensureAlways(IsValid(actor_config->team_visual_data));

    auto const awareness_scan_tick_period{
        simulation_clock.frequency_to_tick_period(actor_config->awareness_scan_frequency)};
    entity_buffers.for_each([=](auto& data) {
        data.awareness_scan_countdowns.set_tick_value(awareness_scan_tick_period);
    });

    auto const attack_reposition_tick_period{
        simulation_clock.frequency_to_tick_period(actor_config->attack_reposition_frequency)};
    entity_buffers.for_each([=](auto& data) {
        data.attack_reposition_countdowns.set_tick_value(attack_reposition_tick_period);
    });

    auto const fire_cooldown_tick_period{
        simulation_clock.duration_to_tick_period(actor_config->laser.fire_cooldown)};
    entity_buffers.for_each(
        [=](auto& data) { data.attack_cooldowns.set_tick_value(fire_cooldown_tick_period); });

    auto const attack_retry_cooldown_tick_period{
        simulation_clock.duration_to_tick_period(actor_config->attack_retry_cooldown)};
    check(FTickCountdown16::tick_can_fit(attack_retry_cooldown_tick_period));
    attack_retry_cooldown_tick_value =
        static_cast<FTickCountdown16::counter_type>(attack_retry_cooldown_tick_period);

    check(actor_config->attack_distance_band.values_are_valid());

    configure_ismc();

    debug_drawer = actor_config->debug_drawer;
    debug_drawer.world = GetWorld();
}

void ATestCapitalShipFighters::begin_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::begin_tick);

    auto& data{entity_buffers.current()};

    data.awareness_scan_countdowns.tick();
    data.attack_reposition_countdowns.tick();
    ml::fill(data.velocities, 0.f);
    clear_tick_buffers();
}
void ATestCapitalShipFighters::update_timers(float const) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::update_timers);

    entity_buffers.current().attack_cooldowns.tick();
}
void ATestCapitalShipFighters::make_decisions() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::make_decisions);

    auto& data{entity_buffers.current()};

    auto const awareness_radius{actor_config->awareness_radius};
    auto const attack_engagement_threshold{actor_config->attack_engagement_threshold};
    auto const attack_engagement_threshold_sq{attack_engagement_threshold *
                                              attack_engagement_threshold};

    auto const n{data.num()};
    TStaticArray<FRegistryEntityHandle, 128> nearby_entities;

    auto const dot_threshold{actor_config->minimum_opportunistic_intercept_deviation_dot_product};

    for (int32 i{0}; i < n; ++i) {
        if (!data.awareness_scan_countdowns.try_consume(i)) {
            continue;
        }

        auto const fighter_location{ml::get_vector3f(data.locations, i)};
        auto const target_handle{data.target_handles[i]};

        if (entity_registry->is_valid_alive(target_handle)) {
            if (data.target_distance_sq[i] <= attack_engagement_threshold_sq) {
                continue;
            }
        }

        auto const n_nearby_entities{spatial_query_manager->collect_non_team_entities_in_range(
            fighter_location, data.teams[i], awareness_radius, nearby_entities)};

        auto const aim_direction{ml::get_vector3f(data.aim_directions, i)};
        for (int32 j{0}; j < n_nearby_entities; ++j) {
            auto const potential_target{nearby_entities[j]};
            auto const potential_target_location{entity_registry->get_location(potential_target)};
            auto const direction_to_target{
                (potential_target_location - fighter_location).GetSafeNormal()};

            if (FVector3f::DotProduct(aim_direction, direction_to_target) > dot_threshold) {
                data.target_handles[i] = potential_target;
                break;
            }
        }
    }
}
void ATestCapitalShipFighters::move(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::move_ships);

    auto const d_turn{FMath::Min(1.f, actor_config->turn_speed_unitless * dt)};

    auto& data{entity_buffers.current()};

    if (data.num() < 1) {
        return;
    }

    auto const& move_view{get_task_view(Task::MoveToDestination)};
    auto const& attack_view{get_task_view(Task::Attack)};

    auto const do_move{move_view.num() > 0};

    auto const n_attack{attack_view.num()};
    auto const do_attack{n_attack > 0};

    auto const laser_max_distance{actor_config->laser.max_distance};
    auto const& attack_distance_band{actor_config->attack_distance_band};
    auto const desired_attack_distance{laser_max_distance * attack_distance_band.desired_ratio};
    auto const inner_attack_distance{laser_max_distance * attack_distance_band.minimum_ratio};
    auto const outer_attack_distance{laser_max_distance * attack_distance_band.maximum_ratio};

    if (do_attack) {
        ml::solve_intercept_times(attack_view.intercept_times,
                                  attack_view.locations.get_const_view(),
                                  attack_view.target_locations.get_const_view(),
                                  attack_view.target_velocities.get_const_view(),
                                  actor_config->laser.projectile_speed);

        for (int32 i{0}; i < n_attack; ++i) {
            auto const intercept_location{ml::get_vector3f(attack_view.target_locations, i) +
                                          ml::get_vector3f(attack_view.target_velocities, i) *
                                              attack_view.intercept_times[i]};
            auto const desired_firing_direction{
                (intercept_location - ml::get_vector3f(attack_view.locations, i)).GetSafeNormal()};

            attack_view.desired_aiming_directions.xs[i] = desired_firing_direction.X;
            attack_view.desired_aiming_directions.ys[i] = desired_firing_direction.Y;
            attack_view.desired_aiming_directions.zs[i] = desired_firing_direction.Z;

            if (!attack_view.attack_reposition_countdowns.try_consume(i)) {
                continue;
            }

            auto const target_to_move_distance{
                FVector3f::Dist(ml::get_vector3f(attack_view.target_locations, i),
                                ml::get_vector3f(attack_view.desired_move_locations, i))};
            auto const is_valid_attack_position{
                (target_to_move_distance >= inner_attack_distance) &&
                (target_to_move_distance <= outer_attack_distance)};
            if (is_valid_attack_position) {
                continue;
            }

            auto const target_direction{(ml::get_vector3f(attack_view.target_locations, i) -
                                         ml::get_vector3f(attack_view.locations, i))
                                            .GetSafeNormal()};
            ml::assign(attack_view.target_directions, i, target_direction);
            ml::assign(attack_view.desired_move_locations,
                       i,
                       ml::get_vector3f(attack_view.target_locations, i) -
                           target_direction * desired_attack_distance);
        }
    }

    // Update movement direction and remaining distance to the movement destination
    ml::direction_and_distance(
        data.movement_directions, data.move_distances, data.locations, data.desired_move_locations);

    // Look phase
    if (do_move) {
        // Look at movement destination
        ml::lerp_in_place(move_view.aim_directions, move_view.movement_directions, d_turn);
    }
    if (do_attack) {
        // Look towards the firing intercept
        ml::lerp_in_place(
            attack_view.aim_directions, attack_view.desired_aiming_directions, d_turn);
    }

    // Move phase
    move(dt, move_view);
    move(dt, attack_view);

    ml::dist_and_dist_sq(attack_view.target_distances,
                         attack_view.target_distance_sq,
                         attack_view.locations,
                         attack_view.target_locations);
}
void ATestCapitalShipFighters::queue_commands() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::queue_commands);

    handle_firing(get_task_view(Task::Attack));
}
void ATestCapitalShipFighters::resolve_damage_events() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::resolve_damage_events);

    auto& data{entity_buffers.current()};

    ml::batch::resolve_damage_events(*entity_registry,
                                     data.entity_handles,
                                     data.healths,
                                     local_indices_to_remove,
                                     entity_death_info);

    auto const& direct_damage{entity_registry->get_direct_damage_queue_view()};
    auto const n_direct_damage{direct_damage.num()};
    for (int32 i{0}; i < n_direct_damage; ++i) {
        auto const local_index{data.entity_handles.Find(direct_damage.damaged_entities[i])};
        if (local_index == INDEX_NONE) {
            continue;
        }

        data.target_handles[local_index] = direct_damage.instigators[i];
    }

    validate_array_sizes();
}
void ATestCapitalShipFighters::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::update_entity_registry);

    prepare_entity_update_data();
    FTestEntityRegistry::ConstView view{entity_buffers.current().entity_handles,
                                        registry_update_data.get_const_view()};
    entity_registry->queue_entity_updates(view, entity_death_info);
}
void ATestCapitalShipFighters::sync_from_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::sync_from_registry);

    tasks_are_contiguous(); // Should be true before pruning

    // Prune
    remove_dead_entities();

    // Spawn
    commit_spawns();

    // Update
    commit_orders();
    refresh_target_data();
    if (!tasks_are_contiguous()) {
        refresh_layout();
    }
    refresh_task_views();

    checkCode(entity_buffers.current().validate_array_sizes());
}
void ATestCapitalShipFighters::update_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::update_visual_data);

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
void ATestCapitalShipFighters::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::end_tick);
    TRACE_COUNTER_SET(SandboxTestFighterCount, get_num_instances());
    visual_log_state();
}

void ATestCapitalShipFighters::visual_log_state() const {
#if ENABLE_VISUAL_LOG
    if (!FVisualLogger::IsRecording()) {
        return;
    }
#else
    return;
#endif

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

    if (auto const msg{ml::report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(actor_config->visual_logger_style),
        })}) {
        UE_LOG(LogSandboxEntities,
               Error,
               TEXT("ATestCapitalShipFighters::visual_log_state UObject ptrs are invalid:\n%s"),
               *msg);
        return;
    }

    auto const& style{*actor_config->visual_logger_style};
    auto const normal_line_thickness{static_cast<uint16>(style.lines.normal_line_thickness)};
    auto const highlighted_line_thickness{
        static_cast<uint16>(style.lines.highlighted_line_thickness)};
    auto const& data{entity_buffers.current()};

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

// Movement
void ATestCapitalShipFighters::move(float const dt, TaskView const& fighters) {
    check(dt > 0.f);

    auto const n{fighters.num()};
    for (int32 i{0}; i < n; ++i) {
        auto const max_move_distance{fighters.speeds[i] * dt};
        fighters.move_distances[i] = FMath::Min(fighters.move_distances[i], max_move_distance);

        auto const velocity{ml::get_vector3f(fighters.movement_directions, i) *
                            (fighters.move_distances[i] / dt)};
        ml::assign(fighters.velocities, i, velocity);
    }

    ml::add_scaled_in_place(fighters.locations,
                            fighters.movement_directions,
                            TConstArrayView<float>{fighters.move_distances},
                            1.f);
}

// Accessors
auto ATestCapitalShipFighters::get_num_instances() const noexcept -> int32 {
    return ml::num(entity_buffers.current().num());
}

auto ATestCapitalShipFighters::get_task_spans() const -> TaskSpans {
    check_fighter_tasks();
    return task_spans;
}
auto ATestCapitalShipFighters::get_task_counts() const -> TaskCounts {
    TaskCounts counts{};

    auto const& data{entity_buffers.current()};
    auto const n_tasks{data.tasks.Num()};

    for (int32 i{}; i < n_tasks; ++i) {
        ++counts[std::to_underlying(data.tasks[i])];
    }

    return counts;
}

auto ATestCapitalShipFighters::get_task_view(Task task) noexcept -> TaskView const& {
    return task_views[std::to_underlying(task)];
}
auto ATestCapitalShipFighters::get_const_task_view(Task task) const noexcept
    -> ConstTaskView const& {
    return const_task_views[std::to_underlying(task)];
}

// Visuals
void ATestCapitalShipFighters::configure_ismc() {
    ml::batch::configure_ismc(*instances,
                              {
                                  .mesh = actor_config->mesh.Get(),
                                  .num_custom_data_floats = n_custom_ismc_floats,
                              });
}
void ATestCapitalShipFighters::prepare_ismc_transforms() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::prepare_ismc_transforms);

    auto const n{get_num_instances()};

    auto const n_jobs{8};
    auto const updates_per_slice{FMath::DivideAndRoundUp(n, n_jobs)};

    auto const update_transforms{[this, updates_per_slice, n, n_jobs](int32 const job_index) {
        auto const& data{entity_buffers.current()};

        auto const begin{job_index * updates_per_slice};
        auto const end{FMath::Min(begin + updates_per_slice, n)};

        for (int32 i{begin}; i < end; ++i) {
            ismc_transforms[i].SetLocation(ml::get_vector3d(data.locations, i));

            auto const dir{ml::get_vector3d(data.aim_directions, i)};
            auto const quat{FQuat::FindBetweenNormals(FVector::ForwardVector, dir)};

            ismc_transforms[i].SetRotation(quat);
        }
    }};

    ParallelFor(n_jobs, update_transforms);
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

    auto const& data{entity_buffers.current()};

    auto const n{get_num_instances()};
    for (int32 i{0}; i < n; ++i) {
        FVector const ship_location{ml::get_vector3d(data.locations, i)};

        if (enable_ship_location_debug_drawing) {
            debug_drawer.draw_sphere(ship_location);
        }

        if (enable_target_debug_drawing) {
            if (!data.target_handles[i].is_valid()) {
                continue;
            }

            auto const target_location{ml::get_vector3d(data.target_locations, i)};
            debug_drawer.draw_line(ship_location, target_location);
        }
    }
}
void ATestCapitalShipFighters::write_ismc_custom_data(int32 const offset, int32 const count) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::write_ismc_custom_data);

    check(count >= 0);
    if (count == 0) {
        return;
    }

    auto const& data{entity_buffers.current()};

    auto const colour_cache{
        UTestTeamVisualData::build_team_colour_cache(actor_config->team_visual_data)};
    custom_data_buffer.SetNumUninitialized(count * n_custom_ismc_floats, EAllowShrinking::No);

    auto const teams_slice{TConstArrayView<ETestTeam>{data.teams}.Slice(offset, count)};
    for (int32 i{0}; i < count; ++i) {
        auto const team{teams_slice[i]};

        // Custom ISMC data
        auto const base{i * n_custom_ismc_floats};
        auto const& colour{colour_cache[team]};
        custom_data_buffer[base + 0] = colour.R;
        custom_data_buffer[base + 1] = colour.G;
        custom_data_buffer[base + 2] = colour.B;
    }

    constexpr bool mark_render_dirty{false};
    instances->SetCustomData(offset, offset + count - 1, custom_data_buffer, mark_render_dirty);
}
void ATestCapitalShipFighters::write_ismc_custom_data() {
    write_ismc_custom_data(0, get_num_instances());
}

// Entity data
void ATestCapitalShipFighters::prepare_entity_update_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::prepare_entity_update_data);

    auto const& data{entity_buffers.current()};
    auto const n{get_num_instances()};

    registry_update_data.reset();
    if (n < 1) {
        return;
    }

    ml::add_uninitialised(registry_update_data, n);

    registry_update_data.locations = data.locations;
    registry_update_data.velocities = data.velocities;
    registry_update_data.healths = data.healths;
    registry_update_data.teams = data.teams;
    for (int32 i{0}; i < n; ++i) {
        registry_update_data.alive[i] = static_cast<uint8>(data.healths[i] > 0);
    }

    registry_update_data.validate_array_sizes();
}
bool ATestCapitalShipFighters::tasks_are_contiguous() const noexcept {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::tasks_are_contiguous);

    auto current_task_group{Task::Standby};
    TaskSpans checked_task_spans{};

    auto const& data{entity_buffers.current()};
    auto const n_tasks{data.tasks.Num()};

    for (int32 i{}; i < n_tasks; ++i) {
        auto const task{data.tasks[i]};

        if (task > current_task_group) {
            current_task_group = task;
        } else if (task < current_task_group) {
            return false;
        }
    }

    return false;
}
void ATestCapitalShipFighters::refresh_layout() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::refresh_layout);

    auto const task_counts{get_task_counts()};
    auto const n_fighters{get_num_instances()};

    TaskCounts write_indexes{};

    {
        int32 offset{0};

        for (int32 i{0}; i < n_task_types; ++i) {
            auto const count{task_counts[i]};

            write_indexes[i] = offset;

            task_spans[i].offset = offset;
            task_spans[i].count = count;

            offset += count;
        }

        check(offset == n_fighters);
    }

    entity_buffers.cycle();
    auto const& old_data{entity_buffers.previous()};
    auto& new_data{entity_buffers.current()};
    new_data.reset();
    new_data.add_uninitialised(n_fighters);

    check(old_data.num() == new_data.num());

    for (int32 i{0}; i < n_fighters; i++) {
        auto const task{old_data.tasks[i]};
        auto const task_value{std::to_underlying(task)};

        auto const write_index{write_indexes[task_value]++};

        static_assert(ml::SupportsCopyElement<TArray<ETestCapitalShipFightersTask>>, "Copy");

        new_data.copy_element(write_index, old_data, i);
    }

    check_fighter_tasks();
}

// Spawning
void ATestCapitalShipFighters::queue_spawns(TestCapitalShipFighterSpawnQueue const& new_spawns) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::queue_spawns);

    spawn_queue.append_from(new_spawns);
}
void ATestCapitalShipFighters::commit_spawns() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::commit_spawns);

    ml::reset(new_spawn_entity_handles, new_spawn_entity_data);

    auto const& new_locations{spawn_queue.locations};
    auto const& new_rotations{spawn_queue.rotations};
    auto const& new_teams{spawn_queue.teams};
    auto const& new_targets{spawn_queue.targets};

    auto& data{entity_buffers.current()};

    auto const n_cur{get_num_instances()};
    auto const n_new{ml::num(new_locations)};

    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(new_locations),
        SANDBOX_NAMED_NUM(new_rotations),
        SANDBOX_NAMED_NUM(new_teams),
        SANDBOX_NAMED_NUM(new_targets),
    });
    if (n_new < 1) {
        return;
    }

    auto const speed{actor_config->speed};

    // entity_handles handles later
    ml::append_n(data.tasks, Task::Attack, n_new);
    ml::append_from(data.locations, new_locations);
    ml::append_from(data.desired_move_locations, new_locations);
    data.movement_directions.add_zeroed(n_new);
    data.velocities.add_zeroed(n_new);
    data.move_distances.AddZeroed(n_new);
    ml::add_uninitialised(data.aim_directions, n_new);
    ml::append_n(data.speeds, speed, n_new);
    data.teams.Append(new_teams);
    ml::append_n(data.healths, actor_config->health, n_new);
    data.awareness_scan_countdowns.add_zeroed(n_new);
    data.attack_reposition_countdowns.add_zeroed(n_new);

    data.target_handles.Append(new_targets);
    data.target_locations.add_zeroed(n_new);
    data.target_velocities.add_zeroed(n_new);
    data.target_directions.add_zeroed(n_new);
    data.intercept_times.AddZeroed(n_new);
    data.desired_aiming_directions.add_zeroed(n_new);
    data.target_distance_sq.AddZeroed(n_new);
    data.target_distances.AddZeroed(n_new);
    data.target_radii.AddZeroed(n_new);

    data.attack_cooldowns.add_zeroed(n_new);

    // Fill entity data and set aim_directions
    new_spawn_entity_data.add_uninitialised(n_new);

    ml::fill(new_spawn_entity_data.radii, ml::get_mesh_sphere_bounds(*instances));
    ml::fill(new_spawn_entity_data.alive, uint8{1});

    for (int32 i{0}; i < n_new; ++i) {
        auto const index{n_cur + i};

        auto const direction{ml::get_vector3f(new_rotations, i)};

        ml::assign(data.aim_directions, index, direction);
        ml::assign_from(new_spawn_entity_data.locations, i, data.locations, index);
        new_spawn_entity_data.healths[i] = data.healths[index];
        new_spawn_entity_data.teams[i] = data.teams[index];
    }
    new_spawn_entity_data.set_all_entity_types(ETestEntityType::CapitalShipFighter);

    ml::fill(new_spawn_entity_data.velocities, 0.f);

    // Entity handles
    new_spawn_entity_handles =
        entity_registry->add_entities(new_spawn_entity_data.get_const_view());
    new_spawn_entity_handles.registry_handles.append_to(data.entity_handles);
    data.integral_biases.AddUninitialized(n_new);
    data.float_biases.AddUninitialized(n_new);
    ml::make_deterministic_biases(
        TConstArrayView<int32>{new_spawn_entity_handles.registry_handles.registry_indices},
        TConstArrayView<int32>{new_spawn_entity_handles.registry_handles.generations},
        TArrayView<uint32>{data.integral_biases}.Slice(n_cur, n_new),
        TArrayView<float>{data.float_biases}.Slice(n_cur, n_new));

    // ISMC transforms
    ismc_transforms.AddDefaulted(n_new);
    for (int32 i{0}; i < n_new; ++i) {
        auto const idx{n_cur + i};
        ismc_transforms[idx].SetLocation(ml::get_vector3d(data.locations, idx));
    }

    instances->AddInstances(
        TArray<FTransform>{ismc_transforms.GetData() + n_cur, n_new}, is_world_space, false);
    write_ismc_custom_data(n_cur, n_new);

    validate_array_sizes();
}

// Destruction
void ATestCapitalShipFighters::self_destruct_fighter(FRegistryEntityHandle const handle) {
    auto& data{entity_buffers.current()};

    auto const index{data.entity_handles.Find(handle)};
    check(index != INDEX_NONE);

    data.healths[index] = 0;
    if (!local_indices_to_remove.Contains(index)) {
        local_indices_to_remove.Add(index);
    }
}
void ATestCapitalShipFighters::remove_dead_entities() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::remove_dead_entities);
    auto& data{entity_buffers.current()};

    ml::batch::sort_and_deduplicate_removal_indices(local_indices_to_remove);

    ml::remove_at_swap_many_sorted_desc(local_indices_to_remove, ismc_transforms, data);

    // Remove ISMC instances
    if (local_indices_to_remove.Num()) {
        constexpr bool is_reverse_sorted{true};
        instances->RemoveInstances(local_indices_to_remove, is_reverse_sorted);
    }

    validate_array_sizes();
}

// Combat
void ATestCapitalShipFighters::handle_firing(TaskView const& data) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::handle_firing);

    static FName const socket_name{TEXT("Gun")};

    auto const n_ships{ml::num(data)};
    auto const fire_point_distance{static_cast<float>(
        instances->GetSocketTransform(socket_name, RTS_Component).GetLocation().Size())};
    auto const aim_threshold{fire_dot_product_threshold};

    auto const laser_damage{actor_config->laser.damage};
    auto const laser_speed{actor_config->laser.projectile_speed};
    auto const laser_max_distance{actor_config->laser.max_distance};
    auto const laser_max_distance_sq{laser_max_distance * laser_max_distance};
    auto const desired_attack_distance{laser_max_distance *
                                       actor_config->attack_distance_band.desired_ratio};
    auto const arrival_distance{actor_config->arrival_distance};
    auto const attack_position_arrival_distance_sq{arrival_distance * arrival_distance};

    auto const colour_cache{
        UTestTeamVisualData::build_team_colour_cache(actor_config->team_visual_data)};

    auto& can_fire{scratch_int_buffer};

    ml::reset(new_lasers, aiming_dot_product_buffer, can_fire);
    ml::add_uninitialised(n_ships, new_lasers, aiming_dot_product_buffer);

    ml::dot_product(aiming_dot_product_buffer, data.aim_directions, data.desired_aiming_directions);

    for (int32 ship_index{0}; ship_index < n_ships; ++ship_index) {
        if (!data.attack_cooldowns.is_ready(ship_index)) {
            continue;
        }

        if ((data.target_distance_sq[ship_index] > laser_max_distance_sq) ||
            (!data.target_handles[ship_index].is_valid()) ||
            (aiming_dot_product_buffer[ship_index] < aim_threshold)) {
            data.attack_cooldowns.set_counter(ship_index, attack_retry_cooldown_tick_value);
            continue;
        }

        can_fire.Add(ship_index);
    }

    if (can_fire.IsEmpty()) {
        return;
    }

    // Perform LOS checks to see if we can fire
    [&] {
        auto const n_can_fire{can_fire.Num()};
        auto const los_check_buffer{actor_config->los_check_buffer};

        line_of_sight_starts.set_num(n_can_fire, EAllowShrinking::No);
        line_of_sight_ends.set_num(n_can_fire, EAllowShrinking::No);
        line_of_sight_results.SetNumUninitialized(n_can_fire, EAllowShrinking::No);

        for (int32 i{}; i < n_can_fire; ++i) {
            auto const ship_index{can_fire[i]};
            auto const ship_location{ml::get_vector3f(data.locations, ship_index)};
            auto const direction{ml::get_vector3f(data.aim_directions, ship_index)};
            auto const start{ship_location + direction * fire_point_distance};
            auto const trace_end_offset{los_check_buffer + data.target_radii[ship_index]};
            auto const end_offset{direction * trace_end_offset};
            auto const end{ml::get_vector3f(data.target_locations, ship_index) - end_offset};

            line_of_sight_starts.set(i, start);
            line_of_sight_ends.set(i, end);
        }

        spatial_query_manager->have_clear_lines(line_of_sight_starts.get_const_view(),
                                                line_of_sight_ends.get_const_view(),
                                                line_of_sight_results);

        for (int32 i{n_can_fire - 1}; i >= 0; --i) {
            auto const ship_index{can_fire[i]};
            auto const trace_end_offset{los_check_buffer + data.target_radii[ship_index]};

            if (line_of_sight_results[i] == 0) {
                can_fire.RemoveAtSwap(i, EAllowShrinking::No);
                data.attack_cooldowns.set_counter(ship_index, attack_retry_cooldown_tick_value);

                auto const fighter_location{ml::get_vector3f(data.locations, ship_index)};
                auto const desired_move_location{
                    ml::get_vector3f(data.desired_move_locations, ship_index)};
                auto const has_arrived{
                    FVector3f::DistSquared(fighter_location, desired_move_location) <=
                    attack_position_arrival_distance_sq};
                if (!has_arrived) {
                    continue;
                }

                auto const candidate{
                    find_appropriate_fire_point(*spatial_query_manager,
                                                ml::get_vector3f(data.target_locations, ship_index),
                                                desired_move_location,
                                                fire_point_distance,
                                                trace_end_offset,
                                                desired_attack_distance,
                                                data.integral_biases[ship_index],
                                                data.float_biases[ship_index])};
                if (candidate.IsSet()) {
                    ml::assign(data.desired_move_locations, ship_index, *candidate);
                }
            }
        }
    }();

    auto const n_can_fire{can_fire.Num()};
    ml::set_num(new_lasers, n_can_fire, EAllowShrinking::No);
    int32 write_index{0};
    for (int32 i{0}; i < n_can_fire; ++i) {
        auto const ship_index{can_fire[i]};

        auto const ship_location{ml::get_vector3f(data.locations, ship_index)};
        auto const direction{ml::get_vector3f(data.aim_directions, ship_index)};

        auto const laser_location{ship_location + direction * fire_point_distance};

        ml::assign(new_lasers.locations, i, laser_location);
        ml::assign(new_lasers.rotations, i, direction.ToOrientationRotator());
        auto const base_velocity{ml::get_vector3f(data.velocities, ship_index)};
        ml::assign(new_lasers.base_velocities, i, base_velocity);
        new_lasers.instigator_handles[i] = data.entity_handles[ship_index];
        new_lasers.colours[i] = colour_cache[data.teams[ship_index]];

        data.attack_cooldowns.restart_counter(ship_index);
    }

    new_lasers.set_damages(laser_damage);
    new_lasers.set_speeds(laser_speed);
    new_lasers.set_max_distances(laser_max_distance);

    laser_actor->queue_laser_spawns(new_lasers);
}

// Orders
void ATestCapitalShipFighters::refresh_task_views() {
    auto const n{task_spans.Num()};
    auto& data{entity_buffers.current()};

    for (int32 i{0}; i < n; ++i) {
        auto const span{task_spans[i]};
        const_task_views[i] = data.get_const_view(span.offset, span.count);
        task_views[i] = data.get_view(span.offset, span.count);
    }
}
void ATestCapitalShipFighters::queue_orders(TestCapitalShipFighterOrderQueue const& queue) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::queue_orders);

    order_queue.append_from(queue);
}
void ATestCapitalShipFighters::commit_orders() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::commit_orders);

    auto& data{entity_buffers.current()};

    auto const n_orders{ml::num(order_queue)};
    if (n_orders < 1) {
        return;
    }

    auto& index_buffer{scratch_int_buffer};
    index_buffer.Reset();
    index_buffer.SetNumUninitialized(n_orders, EAllowShrinking::No);

    for (int32 i{0}; i < n_orders; ++i) {
        index_buffer[i] = data.entity_handles.Find(order_queue.handles[i]);
    }

    for (int32 i{0}; i < n_orders; ++i) {
        auto const fighter_index{index_buffer[i]};
        if (fighter_index == INDEX_NONE) {
            continue;
        }

        auto const order{order_queue.orders[i]};
        if (order.task) {
            auto const old_task{data.tasks[fighter_index]};
            auto const new_task{order_queue.tasks[i]};
            data.tasks[fighter_index] = new_task;

            if ((old_task != Task::Attack) && (new_task == Task::Attack)) {
                ml::assign_from(
                    data.desired_move_locations, fighter_index, data.locations, fighter_index);
                data.attack_reposition_countdowns.zero_counter(fighter_index);
            }
        }
        if (order.target) {
            data.target_handles[fighter_index] = order_queue.targets[i];
        }
    }
}

// Targets
void ATestCapitalShipFighters::refresh_target_data() {
    auto& data{entity_buffers.current()};

    entity_registry->refresh_entity_data(data.target_handles,
                                         data.target_locations.get_view(),
                                         data.target_velocities.get_view(),
                                         data.target_radii);

    ml::dist_and_dist_sq(
        data.target_distances, data.target_distance_sq, data.locations, data.target_locations);
}

// Misc
void ATestCapitalShipFighters::clear_runtime_state() {
    auto& data{entity_buffers.current()};
    instances->ClearInstances();

    ml::reset(local_indices_to_remove, data, new_lasers);
}
void ATestCapitalShipFighters::clear_tick_buffers() {
    ml::reset(local_indices_to_remove, new_lasers, entity_death_info, spawn_queue, order_queue);
}

// Checks
#if DO_CHECK
void ATestCapitalShipFighters::validate_array_sizes() const {
    auto const& data{entity_buffers.current()};

    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(data),
        SANDBOX_NAMED_NUM(ismc_transforms),
        SANDBOX_NAMED_NUM(instances->GetNumInstances()),
    });

    data.validate_array_sizes();
}
void ATestCapitalShipFighters::check_fighter_tasks() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestCapitalShipFighters::check_fighter_tasks);

    auto current_task_group{Task::Standby};
    TaskSpans checked_task_spans{};

    auto const& data{entity_buffers.current()};
    auto const n_tasks{data.tasks.Num()};

    for (int32 i{}; i < n_tasks; ++i) {
        auto const task{data.tasks[i]};
        auto const task_value{std::to_underlying(task)};

        if (task == current_task_group) {
            ++checked_task_spans[task_value].count;
            continue;
        } else if (task > current_task_group) {
            current_task_group = task;
            checked_task_spans[task_value].offset = i;
            checked_task_spans[task_value].count = 1;
        } else {
            UE_LOG(LogSandbox,
                   Fatal,
                   TEXT("Found task %s when current group was %s"),
                   *ml::to_string_without_type_prefix(task),
                   *ml::to_string_without_type_prefix(current_task_group));
        }
    }

    // Set offset for any remaining tasks
    auto const n_fighters{get_num_instances()};
    for (int32 i{1}; i < n_task_types; ++i) {
        auto const last_span{checked_task_spans[i - 1]};
        auto const last_end{last_span.end()};
        auto& cur_span{checked_task_spans[i]};

        if (cur_span.offset < last_end) {
            cur_span.offset = last_end;
            check(cur_span.count == 0);
        }
    }

    if (checked_task_spans != task_spans) {
        FString msg{"Incorrect task spans."};
        for (int32 i{0}; i < n_task_types; ++i) {
            msg += FString::Printf(TEXT("\n    %s: Exp: %s, Got: %s"),
                                   *ml::to_string_without_type_prefix(static_cast<Task>(i)),
                                   *task_spans[i].to_compact_string(),
                                   *checked_task_spans[i].to_compact_string());
        }

        UE_LOG(LogSandbox, Fatal, TEXT("%s"), *msg);
    }
}
#endif
