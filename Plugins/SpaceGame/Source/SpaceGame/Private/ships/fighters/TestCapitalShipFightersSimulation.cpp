#include "SpaceGame/ships/fighters/TestCapitalShipFightersSimulation.h"

#include <SpaceGame/entities/TestBatchActorCore.h>
#include <SpaceGame/simulation/LevelSimulationConfig.h>
#include <SpaceGame/simulation/SpatialQueryManager.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/projectile_intercept.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxNative/deterministic_bias.h>

#include <Misc/Optional.h>
#include <ProfilingDebugging/CountersTrace.h>

#include <array>

TRACE_DECLARE_INT_COUNTER(SandboxTestFighterCount, TEXT("Sandbox/TestFighterCount"));

namespace {
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
} // namespace

namespace ml::test_capital_ship_fighters {
void Simulation::set_config(FFighterSimulationConfig const& new_config) noexcept {
    config = new_config;
}

void Simulation::bind_simulation_clock(FSimulationClock const& clock) noexcept {
    simulation_clock.bind(clock);
}

void Simulation::set_entity_registry(FTestEntityRegistry& new_entity_registry) noexcept {
    entity_registry = &new_entity_registry;
}

void Simulation::set_spatial_query_manager(FSpatialQueryManager const& new_query_manager) noexcept {
    spatial_query_manager = &new_query_manager;
}

void Simulation::set_laser_simulation(ml::test_lasers::Simulation& new_simulation) noexcept {
    laser_simulation = &new_simulation;
}

void Simulation::clear_runtime_state() {
    ml::reset(local_indices_to_remove,
              entity_buffers.current(),
              entity_buffers.previous(),
              new_lasers,
              spawn_queue,
              order_queue,
              new_spawn_entity_data,
              new_spawn_entity_handles);
    task_spans = {};
    task_views = {};
    const_task_views = {};
    refresh_task_views();
    clear_presentation_events();
}

void Simulation::begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ship_fighters::Simulation::begin_play);
    TRACE_COUNTER_SET(SandboxTestFighterCount, 0);
    check(entity_registry);
    check(spatial_query_manager);
    check(laser_simulation);
    check(simulation_clock.is_valid());
    check(collision_radius > 0.f);
    check(fire_point_distance >= 0.f);

    auto const awareness_scan_tick_period{
        simulation_clock.frequency_to_tick_period(config.awareness_scan_frequency)};
    entity_buffers.for_each([=](auto& data) {
        data.awareness_scan_countdowns.set_tick_value(awareness_scan_tick_period);
    });

    auto const attack_reposition_tick_period{
        simulation_clock.frequency_to_tick_period(config.attack_reposition_frequency)};
    entity_buffers.for_each([=](auto& data) {
        data.attack_reposition_countdowns.set_tick_value(attack_reposition_tick_period);
    });

    auto const fire_cooldown_tick_period{
        simulation_clock.duration_to_tick_period(config.laser.fire_cooldown)};
    entity_buffers.for_each(
        [=](auto& data) { data.attack_cooldowns.set_tick_value(fire_cooldown_tick_period); });

    auto const attack_retry_cooldown_tick_period{
        simulation_clock.duration_to_tick_period(config.attack_retry_cooldown)};
    check(FTickCountdown16::tick_can_fit(attack_retry_cooldown_tick_period));
    attack_retry_cooldown_tick_value =
        static_cast<FTickCountdown16::counter_type>(attack_retry_cooldown_tick_period);

    check(config.attack_distance_band.values_are_valid());
}

void Simulation::begin_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ship_fighters::Simulation::begin_tick);

    auto& data{entity_buffers.current()};
    data.awareness_scan_countdowns.tick();
    data.attack_reposition_countdowns.tick();
    ml::fill(data.velocities, 0.f);
    clear_tick_buffers();
    clear_presentation_events();
}

void Simulation::update_timers(float const) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ship_fighters::Simulation::update_timers);
    entity_buffers.current().attack_cooldowns.tick();
}

void Simulation::make_decisions() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ship_fighters::Simulation::make_decisions);

    auto& data{entity_buffers.current()};
    auto const awareness_radius{config.awareness_radius};
    auto const attack_engagement_threshold{config.attack_engagement_threshold};
    auto const attack_engagement_threshold_sq{attack_engagement_threshold *
                                              attack_engagement_threshold};
    auto const n{data.num()};
    TStaticArray<FRegistryEntityHandle, 128> nearby_entities;
    auto const dot_threshold{config.minimum_opportunistic_intercept_deviation_dot_product};

    for (int32 i{0}; i < n; ++i) {
        if (!data.awareness_scan_countdowns.try_consume(i)) {
            continue;
        }

        auto const fighter_location{ml::get_vector3f(data.locations, i)};
        auto const target_handle{data.target_handles[i]};
        if (entity_registry->is_valid_alive(target_handle) &&
            data.target_distance_sq[i] <= attack_engagement_threshold_sq) {
            continue;
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

void Simulation::move(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ship_fighters::Simulation::move);

    auto const d_turn{FMath::Min(1.f, config.turn_speed_unitless * dt)};
    auto& data{entity_buffers.current()};
    if (data.num() < 1) {
        return;
    }

    auto const& move_view{get_task_view(Task::MoveToDestination)};
    auto const& attack_view{get_task_view(Task::Attack)};
    auto const do_move{move_view.num() > 0};
    auto const n_attack{attack_view.num()};
    auto const do_attack{n_attack > 0};
    auto const laser_max_distance{config.laser.max_distance};
    auto const& attack_distance_band{config.attack_distance_band};
    auto const desired_attack_distance{laser_max_distance * attack_distance_band.desired_ratio};
    auto const inner_attack_distance{laser_max_distance * attack_distance_band.minimum_ratio};
    auto const outer_attack_distance{laser_max_distance * attack_distance_band.maximum_ratio};

    if (do_attack) {
        ml::solve_intercept_times(attack_view.intercept_times,
                                  attack_view.locations.get_const_view(),
                                  attack_view.target_locations.get_const_view(),
                                  attack_view.target_velocities.get_const_view(),
                                  config.laser.projectile_speed);

        for (int32 i{0}; i < n_attack; ++i) {
            auto const intercept_location{ml::get_vector3f(attack_view.target_locations, i) +
                                          ml::get_vector3f(attack_view.target_velocities, i) *
                                              attack_view.intercept_times[i]};
            auto const desired_firing_direction{
                (intercept_location - ml::get_vector3f(attack_view.locations, i)).GetSafeNormal()};
            ml::assign(attack_view.desired_aiming_directions, i, desired_firing_direction);

            if (!attack_view.attack_reposition_countdowns.try_consume(i)) {
                continue;
            }

            auto const target_to_move_distance{
                FVector3f::Dist(ml::get_vector3f(attack_view.target_locations, i),
                                ml::get_vector3f(attack_view.desired_move_locations, i))};
            auto const is_valid_attack_position{target_to_move_distance >= inner_attack_distance &&
                                                target_to_move_distance <= outer_attack_distance};
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

    ml::direction_and_distance(
        data.movement_directions, data.move_distances, data.locations, data.desired_move_locations);
    if (do_move) {
        ml::lerp_in_place(move_view.aim_directions, move_view.movement_directions, d_turn);
    }
    if (do_attack) {
        ml::lerp_in_place(
            attack_view.aim_directions, attack_view.desired_aiming_directions, d_turn);
    }

    move(dt, move_view);
    move(dt, attack_view);
    ml::dist_and_dist_sq(attack_view.target_distances,
                         attack_view.target_distance_sq,
                         attack_view.locations,
                         attack_view.target_locations);
}

void Simulation::queue_commands() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ship_fighters::Simulation::queue_commands);
    handle_firing(get_task_view(Task::Attack));
}

void Simulation::resolve_damage_events() {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        Sandbox::test_capital_ship_fighters::Simulation::resolve_damage_events);

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
        if (local_index != INDEX_NONE) {
            data.target_handles[local_index] = direct_damage.instigators[i];
        }
    }

    validate_array_sizes();
}

void Simulation::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        Sandbox::test_capital_ship_fighters::Simulation::update_entity_registry);
    prepare_entity_update_data();
    FTestEntityRegistry::ConstView const view{entity_buffers.current().entity_handles,
                                              registry_update_data.get_const_view()};
    entity_registry->queue_entity_updates(view, entity_death_info);
}

void Simulation::sync_from_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        Sandbox::test_capital_ship_fighters::Simulation::sync_from_registry);

    tasks_are_contiguous();
    remove_dead_entities();
    commit_spawns();
    commit_orders();
    refresh_target_data();
    if (!tasks_are_contiguous()) {
        refresh_layout();
    }
    refresh_task_views();
    checkCode(entity_buffers.current().validate_array_sizes());
}

void Simulation::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ship_fighters::Simulation::end_tick);
    TRACE_COUNTER_SET(SandboxTestFighterCount, get_num_instances());
    validate_array_sizes();
}

void Simulation::move(float const dt, TaskView const& fighters) {
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

auto Simulation::get_num_instances() const noexcept -> int32 {
    return entity_buffers.current().num();
}

auto Simulation::get_view(int32 const offset, int32 const width) -> EntityData::View {
    return entity_buffers.current().get_view(offset, width);
}

auto Simulation::get_const_view(int32 const offset, int32 const width) const
    -> EntityData::ConstView {
    return entity_buffers.current().get_const_view(offset, width);
}

auto Simulation::get_handles() const noexcept -> TConstArrayView<FRegistryEntityHandle> {
    return entity_buffers.current().entity_handles;
}

auto Simulation::has_handle(FRegistryEntityHandle const fighter_handle) const -> bool {
    return find_index(fighter_handle) != INDEX_NONE;
}

auto Simulation::get_target_handles() const noexcept -> TConstArrayView<FRegistryEntityHandle> {
    return entity_buffers.current().target_handles;
}

auto Simulation::get_target_handle(FRegistryEntityHandle const fighter_handle) const noexcept
    -> FRegistryEntityHandle {
    return entity_buffers.current().target_handles[find_index(fighter_handle)];
}

auto Simulation::get_target_location(FRegistryEntityHandle const fighter_handle) const
    -> FVector3f {
    return ml::get_vector3f(entity_buffers.current().target_locations, find_index(fighter_handle));
}

auto Simulation::get_tasks() const -> TConstArrayView<Task> {
    return entity_buffers.current().tasks;
}

auto Simulation::get_teams() const -> TConstArrayView<ETestTeam> {
    return entity_buffers.current().teams;
}

auto Simulation::get_task_spans() const -> TaskSpans {
    check_fighter_tasks();
    return task_spans;
}

auto Simulation::get_task_counts() const -> TaskCounts {
    TaskCounts counts{};
    auto const& data{entity_buffers.current()};
    auto const n_tasks{data.tasks.Num()};
    for (int32 i{}; i < n_tasks; ++i) {
        ++counts[std::to_underlying(data.tasks[i])];
    }
    return counts;
}

auto Simulation::get_task_view(Task const task) noexcept -> TaskView const& {
    return task_views[std::to_underlying(task)];
}

auto Simulation::get_const_task_view(Task const task) const noexcept -> ConstTaskView const& {
    return const_task_views[std::to_underlying(task)];
}

void Simulation::set_target_handle_unchecked(int32 const fighter_index,
                                             FRegistryEntityHandle const new_target) noexcept {
    entity_buffers.current().target_handles[fighter_index] = new_target;
}

void Simulation::set_target_handle(FRegistryEntityHandle const fighter_handle,
                                   FRegistryEntityHandle const new_target) noexcept {
    set_target_handle_unchecked(find_index(fighter_handle), new_target);
}

void Simulation::set_task_unchecked(int32 const index, Task const task) noexcept {
    entity_buffers.current().tasks[index] = task;
}

void Simulation::set_task(FRegistryEntityHandle const handle, Task const task) noexcept {
    set_task_unchecked(find_index(handle), task);
}

auto Simulation::find_index(FRegistryEntityHandle const fighter_handle) const noexcept -> int32 {
    return entity_buffers.current().entity_handles.Find(fighter_handle);
}

auto Simulation::get_task_span(Task const task) const -> FIndexSpan {
    return task_spans[std::to_underlying(task)];
}

void Simulation::prepare_entity_update_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        Sandbox::test_capital_ship_fighters::Simulation::prepare_entity_update_data);

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

bool Simulation::tasks_are_contiguous() const noexcept {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        Sandbox::test_capital_ship_fighters::Simulation::tasks_are_contiguous);

    auto current_task_group{Task::Standby};
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

void Simulation::refresh_layout() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ship_fighters::Simulation::refresh_layout);

    auto const task_counts{get_task_counts()};
    auto const n_fighters{get_num_instances()};
    TaskCounts write_indexes{};
    int32 offset{0};
    for (int32 i{0}; i < n_task_types; ++i) {
        auto const count{task_counts[i]};
        write_indexes[i] = offset;
        task_spans[i].offset = offset;
        task_spans[i].count = count;
        offset += count;
    }
    check(offset == n_fighters);

    entity_buffers.cycle();
    auto const& old_data{entity_buffers.previous()};
    auto& new_data{entity_buffers.current()};
    new_data.reset();
    new_data.add_uninitialised(n_fighters);
    check(old_data.num() == new_data.num());

    for (int32 i{0}; i < n_fighters; ++i) {
        auto const task_value{std::to_underlying(old_data.tasks[i])};
        auto const write_index{write_indexes[task_value]++};
        new_data.copy_element(write_index, old_data, i);
    }
    check_fighter_tasks();
}

void Simulation::queue_spawns(TestCapitalShipFighterSpawnQueue const& new_spawns) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ship_fighters::Simulation::queue_spawns);
    spawn_queue.append_from(new_spawns);
}

void Simulation::commit_spawns() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ship_fighters::Simulation::commit_spawns);

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

    ml::append_n(data.tasks, Task::Attack, n_new);
    ml::append_from(data.locations, new_locations);
    ml::append_from(data.desired_move_locations, new_locations);
    data.movement_directions.add_zeroed(n_new);
    data.velocities.add_zeroed(n_new);
    data.move_distances.AddZeroed(n_new);
    ml::add_uninitialised(data.aim_directions, n_new);
    ml::append_n(data.speeds, config.speed, n_new);
    data.teams.Append(new_teams);
    ml::append_n(data.healths, config.health, n_new);
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

    new_spawn_entity_data.add_uninitialised(n_new);
    ml::fill(new_spawn_entity_data.radii, collision_radius);
    ml::fill(new_spawn_entity_data.alive, uint8{1});
    for (int32 i{0}; i < n_new; ++i) {
        auto const index{n_cur + i};
        ml::assign(data.aim_directions, index, ml::get_vector3f(new_rotations, i));
        ml::assign_from(new_spawn_entity_data.locations, i, data.locations, index);
        new_spawn_entity_data.healths[i] = data.healths[index];
        new_spawn_entity_data.teams[i] = data.teams[index];
    }
    new_spawn_entity_data.set_all_entity_types(ETestEntityType::CapitalShipFighter);
    ml::fill(new_spawn_entity_data.velocities, 0.f);

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

    presentation_spawn_offset = n_cur;
    presentation_spawn_count = n_new;
    validate_array_sizes();
}

void Simulation::self_destruct_fighter(FRegistryEntityHandle const handle) {
    auto& data{entity_buffers.current()};
    auto const index{data.entity_handles.Find(handle)};
    check(index != INDEX_NONE);
    data.healths[index] = 0;
    if (!local_indices_to_remove.Contains(index)) {
        local_indices_to_remove.Add(index);
    }
}

void Simulation::remove_dead_entities() {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        Sandbox::test_capital_ship_fighters::Simulation::remove_dead_entities);
    auto& data{entity_buffers.current()};
    ml::batch::sort_and_deduplicate_removal_indices(local_indices_to_remove);
    presentation_indices_to_remove = local_indices_to_remove;
    ml::remove_at_swap_many_sorted_desc(local_indices_to_remove, data);
    validate_array_sizes();
}

void Simulation::handle_firing(TaskView const& data) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ship_fighters::Simulation::handle_firing);

    auto const n_ships{ml::num(data)};
    auto const aim_threshold{fire_dot_product_threshold};
    auto const laser_damage{config.laser.damage};
    auto const laser_speed{config.laser.projectile_speed};
    auto const laser_max_distance{config.laser.max_distance};
    auto const laser_max_distance_sq{laser_max_distance * laser_max_distance};
    auto const desired_attack_distance{laser_max_distance *
                                       config.attack_distance_band.desired_ratio};
    auto const arrival_distance{config.arrival_distance};
    auto const attack_position_arrival_distance_sq{arrival_distance * arrival_distance};
    auto const colour_cache{config.team_colours};
    auto& can_fire{scratch_int_buffer};

    ml::reset(new_lasers, aiming_dot_product_buffer, can_fire);
    ml::add_uninitialised(n_ships, new_lasers, aiming_dot_product_buffer);
    ml::dot_product(aiming_dot_product_buffer, data.aim_directions, data.desired_aiming_directions);

    for (int32 ship_index{0}; ship_index < n_ships; ++ship_index) {
        if (!data.attack_cooldowns.is_ready(ship_index)) {
            continue;
        }
        if (data.target_distance_sq[ship_index] > laser_max_distance_sq ||
            !data.target_handles[ship_index].is_valid() ||
            aiming_dot_product_buffer[ship_index] < aim_threshold) {
            data.attack_cooldowns.set_counter(ship_index, attack_retry_cooldown_tick_value);
            continue;
        }
        can_fire.Add(ship_index);
    }

    if (can_fire.IsEmpty()) {
        return;
    }

    auto const n_can_fire_before_los{can_fire.Num()};
    auto const los_check_buffer{config.los_check_buffer};
    line_of_sight_starts.set_num(n_can_fire_before_los, EAllowShrinking::No);
    line_of_sight_ends.set_num(n_can_fire_before_los, EAllowShrinking::No);
    line_of_sight_results.SetNumUninitialized(n_can_fire_before_los, EAllowShrinking::No);
    for (int32 i{}; i < n_can_fire_before_los; ++i) {
        auto const ship_index{can_fire[i]};
        auto const ship_location{ml::get_vector3f(data.locations, ship_index)};
        auto const direction{ml::get_vector3f(data.aim_directions, ship_index)};
        auto const start{ship_location + direction * fire_point_distance};
        auto const trace_end_offset{los_check_buffer + data.target_radii[ship_index]};
        auto const end{ml::get_vector3f(data.target_locations, ship_index) -
                       direction * trace_end_offset};
        line_of_sight_starts.set(i, start);
        line_of_sight_ends.set(i, end);
    }

    spatial_query_manager->have_clear_lines(line_of_sight_starts.get_const_view(),
                                            line_of_sight_ends.get_const_view(),
                                            line_of_sight_results);

    for (int32 i{n_can_fire_before_los - 1}; i >= 0; --i) {
        auto const ship_index{can_fire[i]};
        auto const trace_end_offset{los_check_buffer + data.target_radii[ship_index]};
        if (line_of_sight_results[i] != 0) {
            continue;
        }

        can_fire.RemoveAtSwap(i, EAllowShrinking::No);
        data.attack_cooldowns.set_counter(ship_index, attack_retry_cooldown_tick_value);
        auto const fighter_location{ml::get_vector3f(data.locations, ship_index)};
        auto const desired_move_location{ml::get_vector3f(data.desired_move_locations, ship_index)};
        auto const has_arrived{FVector3f::DistSquared(fighter_location, desired_move_location) <=
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

    auto const n_can_fire{can_fire.Num()};
    ml::set_num(new_lasers, n_can_fire, EAllowShrinking::No);
    for (int32 i{0}; i < n_can_fire; ++i) {
        auto const ship_index{can_fire[i]};
        auto const ship_location{ml::get_vector3f(data.locations, ship_index)};
        auto const direction{ml::get_vector3f(data.aim_directions, ship_index)};
        ml::assign(new_lasers.locations, i, ship_location + direction * fire_point_distance);
        ml::assign(new_lasers.rotations, i, direction.ToOrientationRotator());
        ml::assign(new_lasers.base_velocities, i, ml::get_vector3f(data.velocities, ship_index));
        new_lasers.instigator_handles[i] = data.entity_handles[ship_index];
        new_lasers.colours[i] = colour_cache[data.teams[ship_index]];
        data.attack_cooldowns.restart_counter(ship_index);
    }

    new_lasers.set_damages(laser_damage);
    new_lasers.set_speeds(laser_speed);
    new_lasers.set_max_distances(laser_max_distance);
    laser_simulation->queue_laser_spawns(new_lasers);
}

void Simulation::refresh_task_views() {
    auto const n{task_spans.Num()};
    auto& data{entity_buffers.current()};
    for (int32 i{0}; i < n; ++i) {
        auto const span{task_spans[i]};
        const_task_views[i] = data.get_const_view(span.offset, span.count);
        task_views[i] = data.get_view(span.offset, span.count);
    }
}

void Simulation::queue_orders(TestCapitalShipFighterOrderQueue const& queue) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ship_fighters::Simulation::queue_orders);
    order_queue.append_from(queue);
}

void Simulation::commit_orders() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ship_fighters::Simulation::commit_orders);

    auto& data{entity_buffers.current()};
    auto const n_orders{ml::num(order_queue)};
    if (n_orders < 1) {
        return;
    }

    auto& index_buffer{scratch_int_buffer};
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
            if (old_task != Task::Attack && new_task == Task::Attack) {
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

void Simulation::refresh_target_data() {
    auto& data{entity_buffers.current()};
    entity_registry->refresh_entity_data(data.target_handles,
                                         data.target_locations.get_view(),
                                         data.target_velocities.get_view(),
                                         data.target_radii);
    ml::dist_and_dist_sq(
        data.target_distances, data.target_distance_sq, data.locations, data.target_locations);
}

void Simulation::clear_tick_buffers() {
    ml::reset(local_indices_to_remove, new_lasers, entity_death_info, spawn_queue, order_queue);
}

void Simulation::clear_presentation_events() {
    presentation_indices_to_remove.Reset();
    presentation_spawn_offset = 0;
    presentation_spawn_count = 0;
}

#if DO_CHECK
void Simulation::validate_array_sizes() const {
    entity_buffers.current().validate_array_sizes();
}

void Simulation::check_fighter_tasks() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        Sandbox::test_capital_ship_fighters::Simulation::check_fighter_tasks);

    auto current_task_group{Task::Standby};
    TaskSpans checked_task_spans{};
    auto const& data{entity_buffers.current()};
    auto const n_tasks{data.tasks.Num()};
    for (int32 i{}; i < n_tasks; ++i) {
        auto const task{data.tasks[i]};
        auto const task_value{std::to_underlying(task)};
        if (task == current_task_group) {
            ++checked_task_spans[task_value].count;
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

    for (int32 i{1}; i < n_task_types; ++i) {
        auto const last_span{checked_task_spans[i - 1]};
        auto const last_end{last_span.end()};
        auto& current_span{checked_task_spans[i]};
        if (current_span.offset < last_end) {
            current_span.offset = last_end;
            check(current_span.count == 0);
        }
    }

    if (checked_task_spans != task_spans) {
        FString message{TEXT("Incorrect task spans.")};
        for (int32 i{0}; i < n_task_types; ++i) {
            message += FString::Printf(TEXT("\n    %s: Exp: %s, Got: %s"),
                                       *ml::to_string_without_type_prefix(static_cast<Task>(i)),
                                       *task_spans[i].to_compact_string(),
                                       *checked_task_spans[i].to_compact_string());
        }
        UE_LOG(LogSandbox, Fatal, TEXT("%s"), *message);
    }
}
#endif
} // namespace ml::test_capital_ship_fighters
