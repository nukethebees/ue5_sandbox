#include "sandbox/simulation/ships/fighters/TestCapitalShipFightersSimulation.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <sandbox/core/countdown.h>
#include <sandbox/core/diagnostics.h>
#include <sandbox/core/periodic_tick_countdown.h>
#include <sandbox/core/tick_countdown.h>
#include <sandbox/core/vector_math.h>
#include <sandbox/simulation/deterministic_bias.h>
#include <sandbox/simulation/rotator_math.h>
#include <sandbox/simulation/vector_operations.h>
#include <span>
#include <utility>
#include <vector>

#include <sandbox/simulation/combat/lasers/TestLasersFrameScratch.h>
#include <sandbox/simulation/entities/BatchSimulation.h>
#include <sandbox/simulation/entities/NativeEntityRegistryView.h>
#include <sandbox/simulation/fighter_aiming.h>
#include <sandbox/simulation/fighter_attack_preparation.h>
#include <sandbox/simulation/fighter_damage_response.h>
#include <sandbox/simulation/fighter_firing.h>
#include <sandbox/simulation/fighter_firing_scratch.h>
#include <sandbox/simulation/fighter_movement.h>
#include <sandbox/simulation/fighter_navigation_application.h>
#include <sandbox/simulation/fighter_navigation_scans.h>
#include <sandbox/simulation/fighter_navigation_schedule.h>
#include <sandbox/simulation/fighter_orders.h>
#include <sandbox/simulation/fighter_spawn_admission.h>
#include <sandbox/simulation/fighter_spawn_initialization.h>
#include <sandbox/simulation/fighter_targeting.h>
#include <sandbox/simulation/laser_source.h>
#include <sandbox/simulation/simulation/FighterDiagnostics.h>
#include <sandbox/simulation/simulation/FrameTraceHits.h>
#include <sandbox/simulation/simulation/SpatialQueryManager.h>

#include <sandbox/core/frame_array.h>

namespace ml::test_capital_ship_fighters {
namespace diagnostic_detail {
auto vector_string(ml::simulation::Vector3f const value) -> std::string {
    return std::format("({}, {}, {})", value.X, value.Y, value.Z);
}
}

/* **************************************** */
// Navigation helpers
/* **************************************** */
auto Simulation::get_navigation_tick_period(NavigationRiskTier const tier) const -> std::int16_t {
    auto const index{static_cast<std::int32_t>(tier)};
    assert(index >= 0 && static_cast<std::size_t>(index) < navigation_tick_periods.size());
    return navigation_tick_periods[index];
}
auto Simulation::get_native_navigation_state() -> ml::simulation::fighters::NavigationStateView {
    auto& data{entity_buffers.current()};
    auto const count{static_cast<std::size_t>(data.num())};
    return {.separation_steering = data.separation_steering.get_view(),
            .risk_tiers = {data.navigation_risk_tiers.data(), count},
            .lower_risk_scan_counts = {data.navigation_lower_risk_scan_counts.data(), count},
            .choices = {data.avoidance_choice_indices.data(), count},
            .clear_scan_counts = {data.avoidance_clear_scan_counts.data(), count},
            .periods = {data.navigation_update_countdowns_periods.data(), count},
            .remaining_ticks = {data.navigation_update_countdowns_remaining_ticks.data(), count}};
}
void Simulation::reset_navigation_state(std::int32_t const fighter_index,
                                        NavigationRiskTier const initial_tier) {
    ml::simulation::fighters::reset_navigation_state(get_native_navigation_state(),
                                                     fighter_index,
                                                     initial_tier,
                                                     get_navigation_tick_period(initial_tier),
                                                     direct_movement_choice);
}

/* **************************************** */
// Configuration
/* **************************************** */
void Simulation::set_config(
    FFighterSimulationConfig const& new_config,
    std::span<ml::simulation::Team const> const participating_teams) noexcept {
    config = new_config;
    for (auto& is_participant : participant_mask) {
        is_participant = 0;
    }
    for (auto const team : participating_teams) {
        auto const team_index{static_cast<std::int32_t>(team)};
        if (team_index >= 0 && static_cast<std::size_t>(team_index) < participant_mask.size()) {
            participant_mask[team_index] = 1;
        }
    }

    per_team_limit = participating_teams.empty()
                       ? 0
                       : std::max(0, config.max_live_fighters) / participating_teams.size();
}
Simulation::Simulation(FSimulationClock const& clock,
                       FTestEntityRegistry& in_entity_registry,
                       FSpatialQueryManager const& in_spatial_query_manager,
                       ml::test_lasers::Simulation& in_laser_simulation,
                       std::pmr::memory_resource& in_frame_memory_resource) noexcept
    : simulation_clock{clock}
    , entity_registry{in_entity_registry}
    , spatial_query_manager{in_spatial_query_manager}
    , frame_memory_resource{in_frame_memory_resource}
    , laser_simulation{in_laser_simulation} {}

/* **************************************** */
// Simulation phases
/* **************************************** */
void Simulation::begin_play() {
    awareness_cleaner_ = 0;
    reposition_cleaner_ = 0;
    attack_cleaner_ = 0;
    assert(collision_radius > 0.f);
    assert(fire_point_distance >= 0.f);

    auto const awareness_scan_tick_period{
        simulation_clock.frequency_to_tick_period(config.awareness_scan_frequency)};
    assert(awareness_scan_tick_period >= 0 &&
           std::in_range<std::int8_t>(awareness_scan_tick_period));
    awareness_restart_ticks_ = static_cast<std::int8_t>(awareness_scan_tick_period);

    auto const attack_reposition_tick_period{
        simulation_clock.frequency_to_tick_period(config.attack_reposition_frequency)};
    assert(attack_reposition_tick_period >= 0 &&
           std::in_range<std::int16_t>(attack_reposition_tick_period));
    reposition_restart_ticks_ = static_cast<std::int16_t>(attack_reposition_tick_period);

    std::array<float, static_cast<std::int32_t>(NavigationRiskTier::Count)> const frequencies{
        config.avoidance_clear_update_frequency,
        config.avoidance_update_frequency,
        config.avoidance_active_update_frequency,
        config.avoidance_immediate_update_frequency,
    };
    for (std::int32_t i{}; static_cast<std::size_t>(i) < frequencies.size(); ++i) {
        auto const period{simulation_clock.frequency_to_tick_period(frequencies[i])};
        assert(period > 0 && std::in_range<std::int16_t>(period));
        navigation_tick_periods[i] = static_cast<std::int16_t>(period);
    }
    auto const clear_update_interval{
        static_cast<float>(get_navigation_tick_period(NavigationRiskTier::Clear)) *
        simulation_clock.get_tick_period()};
    minimum_navigation_lookahead_time = clear_update_interval * 1.25f;

    auto const fire_cooldown_tick_period{
        simulation_clock.duration_to_tick_period(config.laser.fire_cooldown)};
    assert(fire_cooldown_tick_period >= 0 &&
           std::in_range<std::int16_t>(fire_cooldown_tick_period));
    attack_restart_ticks_ = static_cast<std::int16_t>(fire_cooldown_tick_period);

    auto const attack_retry_cooldown_tick_period{
        simulation_clock.duration_to_tick_period(config.attack_retry_cooldown)};
    assert(attack_retry_cooldown_tick_period >= 0 &&
           std::in_range<std::int16_t>(attack_retry_cooldown_tick_period));
    attack_retry_cooldown_tick_value = static_cast<std::int16_t>(attack_retry_cooldown_tick_period);

    assert(config.attack_distance_band.values_are_valid());
}
void Simulation::begin_tick() {

    auto& data{entity_buffers.current()};
    ml::tick_countdowns<std::int8_t>(data.awareness_scan_countdowns, awareness_cleaner_, 64);
    ml::tick_countdowns<std::int16_t>(
        data.attack_reposition_countdowns, reposition_cleaner_, 16384);
    ml::tick_periodic_countdowns<std::int16_t>(data.navigation_update_countdowns_remaining_ticks);
    data.velocities.each_column([](auto& column) { std::ranges::fill(column, 0.f); });
    clear_tick_buffers();

    for (auto& capacity : remaining_team_capacity) {
        capacity = per_team_limit;
    }
    for (auto const team : data.teams) {
        auto const team_index{static_cast<std::int32_t>(team)};
        if (team_index >= 0 && static_cast<std::size_t>(team_index) < participant_mask.size() &&
            participant_mask[team_index] != 0) {
            remaining_team_capacity[team_index] =
                std::max(0, remaining_team_capacity[team_index] - 1);
        } else {
            ml::log_error(
                std::format("Live fighter has invalid or non-participating team {}", team_index));
        }
    }
}
void Simulation::update_timers(float const) {
    ml::tick_countdowns<std::int16_t>(
        entity_buffers.current().attack_cooldowns, attack_cleaner_, 16384);
}
void Simulation::make_decisions() {

    auto& data{entity_buffers.current()};
    auto const awareness_radius{config.awareness_radius};
    auto const attack_engagement_threshold{config.attack_engagement_threshold};
    auto const attack_engagement_threshold_sq{attack_engagement_threshold *
                                              attack_engagement_threshold};
    auto const n{data.num()};
    std::array<FRegistryEntityHandle, 128> nearby_entities;
    auto const dot_threshold{config.minimum_opportunistic_intercept_deviation_dot_product};
    auto const registry{ml::make_native_query_view(entity_registry)};

    for (std::int32_t i{0}; i < n; ++i) {
        if (!ml::TickCountdownView<std::int8_t>{data.awareness_scan_countdowns,
                                                awareness_restart_ticks_}
                 .try_consume(i)) {
            continue;
        }

        auto const fighter_location{data.locations[i]};
        auto const target_handle{data.target_handles[i]};
        if (entity_registry.is_valid_alive(target_handle) &&
            data.target_distance_sq[i] <= attack_engagement_threshold_sq) {
            continue;
        }

        auto const n_nearby_entities{spatial_query_manager.collect_non_team_entities_in_range(
            fighter_location, data.teams[i], awareness_radius, nearby_entities)};
        auto const aim_direction{data.aim_directions[i]};
        auto const selected_target{ml::simulation::fighters::select_opportunistic_target(
            fighter_location,
            aim_direction,
            {nearby_entities.data(), static_cast<std::size_t>(n_nearby_entities)},
            registry,
            dot_threshold,
            1.e-8f)};
        if (!selected_target.is_null()) {
            data.target_handles[i] = selected_target;
        }
    }
}
void Simulation::move(float const dt) {

    auto const d_turn{std::min(1.f, config.turn_speed_unitless * dt)};
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
        ml::simulation::fighters::prepare_attack(
            {.locations = attack_view.locations.get_const_view(),
             .target_locations = attack_view.target_locations.get_const_view(),
             .target_velocities = attack_view.target_velocities.get_const_view(),
             .intercept_times = {attack_view.intercept_times.data(),
                                 static_cast<std::size_t>(n_attack)},
             .desired_aiming_directions = attack_view.desired_aiming_directions,
             .target_directions = attack_view.target_directions,
             .desired_move_locations = attack_view.desired_move_locations,
             .reposition_countdowns =
                 ml::TickCountdownView<std::int16_t>{attack_view.attack_reposition_countdowns,
                                                     reposition_restart_ticks_}},
            {.projectile_speed = config.laser.projectile_speed,
             .desired_distance = desired_attack_distance,
             .inner_distance = inner_attack_distance,
             .outer_distance = outer_attack_distance,
             .squared_normal_tolerance = 1.e-8f});
    }

    ml::simulation::fighters::prepare_movement(
        data.movement_directions.get_view(),
        {data.move_distances.data(), static_cast<std::size_t>(data.num())},
        data.locations.get_const_view(),
        data.desired_move_locations.get_const_view());
    update_navigation_steering();
    if (do_move) {
        ml::simulation::fighters::update_movement_aiming(
            move_view.aim_directions, move_view.movement_directions.get_const_view(), d_turn);
    }
    if (do_attack) {
        ml::simulation::fighters::update_attack_aiming(
            attack_view.aim_directions,
            attack_view.movement_directions.get_const_view(),
            attack_view.desired_aiming_directions.get_const_view(),
            {attack_view.avoidance_choice_indices.data(), static_cast<std::size_t>(n_attack)},
            d_turn);
    }

    move(dt, move_view);
    move(dt, attack_view);
    ml::simulation::distance_and_squared(attack_view.target_distances,
                                         attack_view.target_distance_sq,
                                         attack_view.locations.get_const_view(),
                                         attack_view.target_locations.get_const_view());
}
void Simulation::queue_commands() {
    handle_firing(get_task_view(Task::Attack));
}
void Simulation::resolve_damage_events() {

    auto& data{entity_buffers.current()};
    ml::batch::resolve_damage_events(entity_registry,
                                     data.entity_handles,
                                     data.healths,
                                     local_indices_to_remove,
                                     entity_death_info);

    auto const& direct_damage{entity_registry.get_direct_damage_queue_view()};
    auto const fighter_count{static_cast<std::size_t>(data.num())};
    simulation::fighters::retarget_from_damage(
        {data.entity_handles.data(), fighter_count},
        std::as_bytes(std::span{data.teams.data(), fighter_count}),
        {data.target_handles.data(), fighter_count},
        direct_damage.get_const_view(),
        ml::make_native_query_view(entity_registry));

    validate_array_sizes();
}
void Simulation::update_entity_registry() {
    prepare_entity_update_data();
    FTestEntityRegistry::ConstView const view{entity_buffers.current().entity_handles,
                                              registry_update_data.get_const_view()};
    entity_registry.queue_entity_updates(view, entity_death_info);
}
void Simulation::sync_from_registry() {

    tasks_are_contiguous();
    remove_dead_entities();
    commit_orders();
    refresh_target_data();
    if (!tasks_are_contiguous()) {
        refresh_layout();
    }
    refresh_task_views();
    validate_array_sizes();
}
void Simulation::end_tick() {
    validate_array_sizes();
}

/* **************************************** */
// Movement
/* **************************************** */
void Simulation::move(float const dt, TaskView const& fighters) {
    assert(dt > 0.f);
    simulation::fighters::move(
        {
            .locations = fighters.locations,
            .directions = fighters.movement_directions.get_const_view(),
            .velocities = fighters.velocities,
            .move_distances = {fighters.move_distances.data(),
                               static_cast<std::size_t>(fighters.move_distances.size())},
            .speeds = {fighters.speeds.data(), static_cast<std::size_t>(fighters.speeds.size())},
        },
        dt);
}
void Simulation::update_navigation_steering() {

    auto const clearance{collision_radius + config.avoidance_clearance_buffer};
    auto const minimum_lookahead_distance{collision_radius * 2.f};
    auto const avoidance_lookahead_time{
        std::max(config.avoidance_lookahead_time, minimum_navigation_lookahead_time)};
    auto const active_update_interval{
        static_cast<float>(get_navigation_tick_period(NavigationRiskTier::Active)) *
        simulation_clock.get_tick_period()};
    auto const safe_progress_time{active_update_interval * 1.25f};
    navigation_telemetry = {};
    NavigationScratch scratch{&frame_memory_resource};

    // Only expired countdowns observe the world. Held steering is applied to every mover.
    collect_navigation_updates(scratch);
    update_separation_observations(scratch);
    apply_separation_steering();

    // Hard sweeps have final authority over the traffic-biased preferred direction.
    scan_preferred_navigation(
        scratch, clearance, avoidance_lookahead_time, minimum_lookahead_distance);
    scan_alternative_navigation(
        scratch, clearance, avoidance_lookahead_time, minimum_lookahead_distance);
    select_navigation_alternatives(scratch, safe_progress_time);
    apply_navigation_choices(scratch);
    publish_navigation_telemetry();
}
void Simulation::collect_navigation_updates(NavigationScratch& scratch) {
    auto& data{entity_buffers.current()};
    std::array const active_spans{get_task_span(Task::MoveToDestination),
                                  get_task_span(Task::Attack)};
    ml::simulation::fighters::collect_navigation_updates(
        active_spans,
        ml::PeriodicTickCountdownView<std::int16_t>{
            data.navigation_update_countdowns_remaining_ticks,
            data.navigation_update_countdowns_periods},
        scratch);
}
void Simulation::update_separation_observations(NavigationScratch& scratch) {
    auto& data{entity_buffers.current()};
    // Fixed capacity bounds scoring work and keeps neighbour storage off the heap.
    std::array<FRegistryEntityHandle, max_separation_neighbours> nearby_fighters;
    auto const separation_radius{config.separation_radius};
    auto const immediate_distance{collision_radius * 2.f};
    auto const close_distance{std::max(separation_radius * 0.5f, immediate_distance)};
    auto const immediate_distance_sq{immediate_distance * immediate_distance};
    auto const close_distance_sq{close_distance * close_distance};
    auto const registry_view{ml::make_native_query_view(entity_registry)};

    for (auto const fighter_index : scratch.ready_fighter_indices) {
        auto const goal_direction{data.movement_directions[fighter_index]};
        auto const move_distance{data.move_distances[fighter_index]};
        if (ml::native_math::is_nearly_zero(
                goal_direction.X, goal_direction.Y, goal_direction.Z, 1.e-4f) ||
            move_distance <= 0.f) {
            data.separation_steering.set(fighter_index, ml::simulation::Vector3f{});
            data.avoidance_choice_indices[fighter_index] = direct_movement_choice;
            data.avoidance_clear_scan_counts[fighter_index] = 0;
            continue;
        }

        auto const fighter_location{data.locations[fighter_index]};
        auto const fighter_handle{data.entity_handles[fighter_index]};
        auto const n_nearby{spatial_query_manager.collect_entities_of_type_in_range(
            fighter_location,
            ml::simulation::EntityType::CapitalShipFighter,
            separation_radius,
            fighter_handle,
            nearby_fighters)};
        ++navigation_telemetry.separation_query_count;
        navigation_telemetry.separation_candidate_count += n_nearby;

        auto const previous_memory{data.separation_steering[fighter_index]};
        auto const current_tier{
            static_cast<NavigationRiskTier>(data.navigation_risk_tiers[fighter_index])};
        auto const elapsed_since_scan{static_cast<float>(get_navigation_tick_period(current_tier)) *
                                      simulation_clock.get_tick_period()};
        auto const memory_retention{
            config.steering_memory_duration > 0.f
                ? static_cast<float>(std::clamp(
                      1.0 - elapsed_since_scan / config.steering_memory_duration, 0.0, 1.0))
                : 0.f};
        auto const observation{simulation::fighters::observe_separation(
            registry_view.locations,
            registry_view.generations,
            fighter_location,
            fighter_handle,
            goal_direction,
            previous_memory,
            {nearby_fighters.data(), static_cast<std::size_t>(n_nearby)},
            {
                .separation_radius = separation_radius,
                .immediate_distance_squared = immediate_distance_sq,
                .close_distance_squared = close_distance_sq,
                .memory_retention = static_cast<float>(memory_retention),
                .separation_strength = config.separation_strength,
                .dense_traffic_neighbour_threshold = config.dense_traffic_neighbour_threshold,
                .integral_bias = data.integral_biases[fighter_index],
                .float_bias = data.float_biases[fighter_index],
            })};
        if (observation.dense_direction_selected) {
            ++navigation_telemetry.dense_direction_selection_count;
        }
        data.separation_steering.set(fighter_index, observation.steering_memory);
        scratch.observed_risk_tiers[fighter_index] =
            static_cast<std::uint8_t>(observation.risk_tier);
    }
}
void Simulation::apply_separation_steering() {
    auto& data{entity_buffers.current()};
    std::array const active_spans{get_task_span(Task::MoveToDestination),
                                  get_task_span(Task::Attack)};
    ml::simulation::fighters::apply_separation_steering(data.movement_directions.get_view(),
                                                        data.separation_steering.get_const_view(),
                                                        active_spans,
                                                        config.separation_strength);
}
void Simulation::scan_preferred_navigation(NavigationScratch& scratch,
                                           float const clearance,
                                           float const avoidance_lookahead_time,
                                           float const minimum_lookahead_distance) {
    auto& data{entity_buffers.current()};
    auto const count{static_cast<std::size_t>(data.num())};
    ml::simulation::fighters::prepare_preferred_navigation(
        {.locations = data.locations.get_const_view(),
         .preferred_directions = data.movement_directions.get_const_view(),
         .move_distances = {data.move_distances.data(), count},
         .speeds = {data.speeds.data(), count},
         .float_biases = {data.float_biases.data(), count},
         .integral_biases = {data.integral_biases.data(), count},
         .choices = {data.avoidance_choice_indices.data(), count}},
        avoidance_lookahead_time,
        minimum_lookahead_distance,
        1.e-4f,
        scratch);

    auto const n_direct_traces{scratch.trace_fighter_indices.num()};
    if (n_direct_traces > 0) {
        execute_navigation_sweeps(scratch, clearance);

        ml::simulation::fighters::resolve_preferred_navigation(
            scratch,
            {data.avoidance_choice_indices.data(), count},
            {data.avoidance_clear_scan_counts.data(), count},
            direct_movement_choice,
            clear_scans_to_end_avoidance);
    }
}
void Simulation::scan_alternative_navigation(NavigationScratch& scratch,
                                             float const clearance,
                                             float const avoidance_lookahead_time,
                                             float const minimum_lookahead_distance) {
    auto const& data{entity_buffers.current()};
    auto const count{static_cast<std::size_t>(data.num())};
    ml::simulation::fighters::prepare_alternative_navigation(
        {.locations = data.locations.get_const_view(),
         .preferred_directions = data.movement_directions.get_const_view(),
         .move_distances = {data.move_distances.data(), count},
         .speeds = {data.speeds.data(), count},
         .float_biases = {data.float_biases.data(), count},
         .integral_biases = {data.integral_biases.data(), count},
         .choices = {data.avoidance_choice_indices.data(), count}},
        avoidance_lookahead_time,
        minimum_lookahead_distance,
        scratch);
    execute_navigation_sweeps(scratch, clearance);
}
void Simulation::execute_navigation_sweeps(NavigationScratch& scratch, float const clearance) {
    auto const trace_count{scratch.line_of_sight_ends.num()};
    if (trace_count == 0) {
        return;
    }
    auto const moving_half_extent{ml::make_vector3f(clearance, clearance, clearance)};
    scratch.line_of_sight_results.set_num(trace_count);
    spatial_query_manager.are_spheres_in_bounds(
        scratch.line_of_sight_ends.get_const_view(),
        clearance,
        {scratch.line_of_sight_results.data(),
         static_cast<std::size_t>(scratch.line_of_sight_results.num())});
    scratch.trace_hits.set_num(trace_count);
    // Fighters contribute soft steering; solid entities (including the parent capital) block.
    spatial_query_manager.sweep_closest_aabbs(scratch.line_of_sight_starts.get_const_view(),
                                              scratch.line_of_sight_ends.get_const_view(),
                                              moving_half_extent,
                                              scratch.trace_hits.get_view(),
                                              {},
                                              ioj::ETraceEntityFilter::ExcludeCapitalShipFighters);
    navigation_telemetry.hard_trace_count += trace_count;
}
void Simulation::select_navigation_alternatives(NavigationScratch& scratch,
                                                float const safe_progress_time) {
    auto& data{entity_buffers.current()};
    if (!diagnostics_enabled) {
        diagnostic_stop_reports = 0;
    }
    auto const n_blocked_fighters{scratch.blocked_fighter_indices.num()};
    for (std::int32_t blocked_index{}; blocked_index < n_blocked_fighters; ++blocked_index) {
        auto const fighter_index{scratch.blocked_fighter_indices[blocked_index]};
        auto const fighter_location{data.locations[fighter_index]};
        // Partial progress must leave room until the next active scan, including its margin.
        auto const safe_progress_distance{data.speeds[fighter_index] * safe_progress_time};

        auto const candidate_begin{blocked_index * n_avoidance_choices};
        auto const candidate_end{candidate_begin + n_avoidance_choices};
        auto const candidate_count{static_cast<std::size_t>(n_avoidance_choices)};
        auto const chosen_choice{simulation::fighters::choose_navigation_alternative(
            fighter_location,
            safe_progress_distance,
            {scratch.trace_choice_indices.data() + candidate_begin, candidate_count},
            {scratch.line_of_sight_results.data() + candidate_begin, candidate_count},
            scratch.trace_hits.hits.view().subspan(candidate_begin, candidate_count),
            scratch.trace_hits.locations.get_const_view().slice(candidate_begin,
                                                                n_avoidance_choices),
            stop_movement_choice)};

        data.avoidance_choice_indices[fighter_index] = chosen_choice;
        if (chosen_choice == stop_movement_choice &&
            fighter_diagnostics::take_report(diagnostics_enabled, diagnostic_stop_reports, 8)) {
            ml::log_error(std::format(
                "[FighterStop] fighterRegistryIndex={} position={} destination={} "
                "preferred={} separation={} clearance={:.2f} safeTravel={:.2f} risk={}",
                data.entity_handles[fighter_index].index,
                diagnostic_detail::vector_string(fighter_location),
                diagnostic_detail::vector_string(data.desired_move_locations[fighter_index]),
                diagnostic_detail::vector_string(data.movement_directions[fighter_index]),
                diagnostic_detail::vector_string(data.separation_steering[fighter_index]),
                collision_radius + config.avoidance_clearance_buffer,
                safe_progress_distance,
                data.navigation_risk_tiers[fighter_index]));
            for (std::int32_t trace_index{candidate_begin}; trace_index < candidate_end;
                 ++trace_index) {
                ml::log_error(std::format(
                    "[FighterStop] choice={} end={} inWorld={} hit={} "
                    "blockerRegistryIndex={} staticIndex={} hitDistance={:.2f}",
                    scratch.trace_choice_indices[trace_index],
                    diagnostic_detail::vector_string(
                        scratch.line_of_sight_ends.get_const_view()[trace_index]),
                    scratch.line_of_sight_results[trace_index],
                    scratch.trace_hits.hits[trace_index],
                    scratch.trace_hits.entities[trace_index].index,
                    scratch.trace_hits.static_geometry_indices[trace_index],
                    scratch.trace_hits.hits[trace_index]
                        ? HMM_LenV3(fighter_location -
                                    scratch.trace_hits.locations.get_const_view()[trace_index])
                        : -1.f));
            }
        }
    }
}
void Simulation::apply_navigation_choices(NavigationScratch const& scratch) {
    auto& data{entity_buffers.current()};
    auto const count{static_cast<std::size_t>(data.num())};
    std::array const active_spans{get_task_span(Task::MoveToDestination),
                                  get_task_span(Task::Attack)};
    ml::simulation::fighters::apply_navigation_choices(
        {.movement_directions = data.movement_directions.get_view(),
         .separation_steering = data.separation_steering.get_const_view(),
         .float_biases = {data.float_biases.data(), count},
         .choices = {data.avoidance_choice_indices.data(), count},
         .risk_tiers = {data.navigation_risk_tiers.data(), count},
         .lower_risk_scan_counts = {data.navigation_lower_risk_scan_counts.data(), count},
         .periods = {data.navigation_update_countdowns_periods.data(), count},
         .remaining_ticks = {data.navigation_update_countdowns_remaining_ticks.data(), count}},
        active_spans,
        {.risk_periods = {navigation_tick_periods.data(),
                          static_cast<std::size_t>(navigation_tick_periods.size())},
         .direct_choice = direct_movement_choice,
         .stop_choice = stop_movement_choice,
         .scans_to_demote = lower_risk_scans_to_demote,
         .steering_zero_tolerance = 1.e-4f},
        scratch,
        navigation_telemetry);
}
void Simulation::publish_navigation_telemetry() const {}

/* **************************************** */
// Accessors
/* **************************************** */
auto Simulation::get_num_instances() const noexcept -> std::int32_t {
    return entity_buffers.current().num();
}
auto Simulation::get_view(std::int32_t const offset, std::int32_t const width) -> EntityData::View {
    return entity_buffers.current().get_view(offset, width);
}
auto Simulation::get_const_view(std::int32_t const offset, std::int32_t const width) const
    -> EntityData::ConstView {
    return entity_buffers.current().get_const_view(offset, width);
}
auto Simulation::get_handles() const noexcept -> std::span<FRegistryEntityHandle const> {
    return entity_buffers.current().entity_handles;
}
auto Simulation::has_handle(FRegistryEntityHandle const fighter_handle) const -> bool {
    return find_index(fighter_handle) != -1;
}
auto Simulation::get_target_handles() const noexcept -> std::span<FRegistryEntityHandle const> {
    return entity_buffers.current().target_handles;
}
auto Simulation::get_target_handle(FRegistryEntityHandle const fighter_handle) const noexcept
    -> FRegistryEntityHandle {
    return entity_buffers.current().target_handles[find_index(fighter_handle)];
}
auto Simulation::get_target_location(FRegistryEntityHandle const fighter_handle) const
    -> ml::simulation::Vector3f {
    return entity_buffers.current().target_locations[find_index(fighter_handle)];
}
auto Simulation::get_tasks() const -> std::span<Task const> {
    return entity_buffers.current().tasks;
}
auto Simulation::get_teams() const -> std::span<ml::simulation::Team const> {
    return entity_buffers.current().teams;
}
auto Simulation::get_task_spans() const -> TaskSpans {
    check_fighter_tasks();
    return task_spans;
}
auto Simulation::get_task_counts() const -> TaskCounts {
    auto const& data{entity_buffers.current()};
    return ml::simulation::fighters::count_tasks(
        {data.tasks.data(), static_cast<std::size_t>(data.tasks.size())});
}
auto Simulation::get_task_view(Task const task) noexcept -> TaskView const& {
    return task_views[std::to_underlying(task)];
}
auto Simulation::get_const_task_view(Task const task) const noexcept -> ConstTaskView const& {
    return const_task_views[std::to_underlying(task)];
}
auto Simulation::find_index(FRegistryEntityHandle const fighter_handle) const noexcept
    -> std::int32_t {
    auto const& handles{entity_buffers.current().entity_handles};
    auto const found{std::ranges::find(handles, fighter_handle)};
    return found == handles.end() ? -1 : static_cast<std::int32_t>(found - handles.begin());
}
auto Simulation::get_task_span(Task const task) const -> FIndexSpan {
    return task_spans[std::to_underlying(task)];
}

/* **************************************** */
// Targets
/* **************************************** */
void Simulation::set_target_handle_unchecked(std::int32_t const fighter_index,
                                             FRegistryEntityHandle const new_target) noexcept {
    entity_buffers.current().target_handles[fighter_index] = new_target;
}
void Simulation::set_target_handle(FRegistryEntityHandle const fighter_handle,
                                   FRegistryEntityHandle const new_target) noexcept {
    set_target_handle_unchecked(find_index(fighter_handle), new_target);
}
void Simulation::refresh_target_data() {
    auto& data{entity_buffers.current()};
    entity_registry.refresh_entity_data(data.target_handles,
                                        data.target_locations.get_view(),
                                        data.target_velocities.get_view(),
                                        data.target_radii);
    ml::simulation::distance_and_squared(data.target_distances,
                                         data.target_distance_sq,
                                         data.locations.get_const_view(),
                                         data.target_locations.get_const_view());
}

/* **************************************** */
// Tasks
/* **************************************** */
void Simulation::set_task_unchecked(std::int32_t const index, Task const task) noexcept {
    auto& data{entity_buffers.current()};
    if (data.tasks[index] == task) {
        return;
    }

    data.tasks[index] = task;
    reset_navigation_state(
        index, task == Task::Standby ? NavigationRiskTier::Clear : NavigationRiskTier::Nearby);
}
void Simulation::set_task(FRegistryEntityHandle const handle, Task const task) noexcept {
    set_task_unchecked(find_index(handle), task);
}
void Simulation::refresh_task_views() {
    auto const n{static_cast<std::int32_t>(task_spans.size())};
    auto& data{entity_buffers.current()};
    for (std::int32_t i{0}; i < n; ++i) {
        auto const span{task_spans[i]};
        const_task_views[i] = data.get_const_view(span.offset, span.count);
        task_views[i] = data.get_view(span.offset, span.count);
    }
}

/* **************************************** */
// Entity data
/* **************************************** */
void Simulation::prepare_entity_update_data() {

    auto const& data{entity_buffers.current()};
    auto const n{get_num_instances()};
    registry_update_data.reset();
    if (n < 1) {
        return;
    }

    registry_update_data.add_uninitialised(n);
    registry_update_data.locations = data.locations;
    registry_update_data.velocities = data.velocities;
    registry_update_data.healths = data.healths;
    registry_update_data.teams = data.teams;
    for (std::int32_t i{0}; i < n; ++i) {
        registry_update_data.alive[i] = static_cast<std::uint8_t>(data.healths[i] > 0);
        registry_update_data.rotations.set(
            i, ml::simulation::direction_to_rotation(data.aim_directions[i]));
    }
    registry_update_data.validate_array_sizes();
}
bool Simulation::tasks_are_contiguous() const noexcept {

    auto const& data{entity_buffers.current()};
    return ml::simulation::fighters::tasks_are_contiguous(
        {data.tasks.data(), static_cast<std::size_t>(data.tasks.size())}, task_spans);
}
void Simulation::refresh_layout() {

    auto const task_counts{get_task_counts()};
    auto const n_fighters{get_num_instances()};
    ml::simulation::fighters::TaskLayout layout{task_counts};
    task_spans = layout.spans();
    assert(task_spans.back().end() == n_fighters);

    entity_buffers.cycle();
    auto const& old_data{entity_buffers.previous()};
    auto& new_data{entity_buffers.current()};
    new_data.reset();
    new_data.add_uninitialised(n_fighters);
    assert(old_data.num() == new_data.num());

    for (std::int32_t i{0}; i < n_fighters; ++i) {
        auto const write_index{layout.next_index(old_data.tasks[i])};
        new_data.copy_element(write_index, old_data, i);
    }
    check_fighter_tasks();
}

/* **************************************** */
// Spawning
/* **************************************** */
auto Simulation::queue_spawns(
    ml::simulation::TestCapitalShipFighterSpawnQueueConstView const new_spawns) -> std::int32_t {
    new_spawns.validate_array_sizes();
    auto const admission{ml::simulation::fighters::admit_spawns(
        std::as_bytes(new_spawns.teams),
        {participant_mask.data(), static_cast<std::size_t>(participant_mask.size())},
        {remaining_team_capacity.data(),
         static_cast<std::size_t>(remaining_team_capacity.size())})};
    if (admission.status == ml::simulation::fighters::SpawnAdmissionStatus::InvalidTeam) {
        ml::log_error(
            std::format("Rejected fighter spawn request for invalid or non-participating team {}",
                        static_cast<std::int32_t>(new_spawns.teams[0])));
        return 0;
    }

    if (admission.status == ml::simulation::fighters::SpawnAdmissionStatus::MixedTeams) {
        ml::log_error("Rejected fighter spawn wave containing multiple teams");
        return 0;
    }

    if (admission.accepted_count > 0) {
        spawn_queue.append_from(new_spawns.left(admission.accepted_count));
    }
    return admission.accepted_count;
}
void Simulation::commit_spawns() {

    if (!diagnostics_enabled) {
        diagnostic_spawn_reports = 0;
    }
    new_spawn_entity_handles.reset();
    new_spawn_entity_data.reset();
    auto& data{entity_buffers.current()};
    auto const n_cur{get_num_instances()};
    auto const n_new{spawn_queue.num()};

    spawn_queue.validate_array_sizes();
    if (n_new < 1) {
        return;
    }

    data.add_defaulted(n_new);
    auto const new_data{data.get_view(n_cur, n_new)};
    auto const count{static_cast<std::size_t>(n_new)};
    ml::simulation::fighters::initialize_spawned_fighters(
        {.tasks = {new_data.tasks.data(), count},
         .locations = new_data.locations,
         .desired_move_locations = new_data.desired_move_locations,
         .aim_directions = new_data.aim_directions,
         .speeds = {new_data.speeds.data(), count},
         .teams = std::as_writable_bytes(std::span{new_data.teams.data(), count}),
         .healths = {new_data.healths.data(), count},
         .parents = {new_data.parent_handles.data(), count},
         .targets = {new_data.target_handles.data(), count},
         .navigation_risk_tiers = {new_data.navigation_risk_tiers.data(), count},
         .avoidance_choices = {new_data.avoidance_choice_indices.data(), count},
         .navigation_periods = {data.navigation_update_countdowns_periods.data() + n_cur, count}},
        spawn_queue.get_const_view(),
        {.speed = config.speed,
         .health = config.health,
         .navigation_period = get_navigation_tick_period(NavigationRiskTier::Nearby),
         .direct_choice = direct_movement_choice});

    new_spawn_entity_data.add_uninitialised(n_new);
    std::ranges::fill(new_spawn_entity_data.radii, collision_radius);
    std::ranges::fill(new_spawn_entity_data.alive, std::uint8_t{1});
    for (std::int32_t i{0}; i < n_new; ++i) {
        auto const index{n_cur + i};
        new_spawn_entity_data.locations.set(i, data.locations[index]);
        new_spawn_entity_data.rotations.set(
            i, ml::simulation::direction_to_rotation(data.aim_directions[index]));
        new_spawn_entity_data.healths[i] = data.healths[index];
        new_spawn_entity_data.teams[i] = data.teams[index];
    }
    std::ranges::fill(new_spawn_entity_data.entity_types,
                      ml::simulation::EntityType::CapitalShipFighter);
    new_spawn_entity_data.velocities.each_column(
        [](auto& column) { std::ranges::fill(column, 0.f); });

    new_spawn_entity_handles = entity_registry.add_entities(new_spawn_entity_data.get_const_view());
    auto const& registry_handles{new_spawn_entity_handles.registry_handles};
    for (std::int32_t i{}; i < n_new; ++i) {
        data.entity_handles[n_cur + i] = {registry_handles.registry_indices[i],
                                          registry_handles.generations[i]};
    }
    if (diagnostics_enabled) {
        for (std::int32_t i{}; i < n_new; ++i) {
            if (!fighter_diagnostics::take_report(
                    diagnostics_enabled, diagnostic_spawn_reports, 64)) {
                break;
            }
            auto const index{n_cur + i};
            ml::log_error(std::format(
                "[FighterSpawn] Committed fighterRegistryIndex={} parentRegistryIndex={} "
                "targetRegistryIndex={} world={}",
                data.entity_handles[index].index,
                data.parent_handles[index].index,
                data.target_handles[index].index,
                diagnostic_detail::vector_string(data.locations[index])));
        }
    }
    ml::make_deterministic_biases(
        std::span<std::int32_t const>{
            new_spawn_entity_handles.registry_handles.registry_indices.data(),
            static_cast<std::size_t>(n_new)},
        std::span<std::int32_t const>{new_spawn_entity_handles.registry_handles.generations.data(),
                                      static_cast<std::size_t>(n_new)},
        std::span<std::uint32_t>{data.integral_biases}.subspan(n_cur, n_new),
        std::span<float>{data.float_biases}.subspan(n_cur, n_new));

    validate_array_sizes();
}

/* **************************************** */
// Destruction
/* **************************************** */
void Simulation::self_destruct_fighter(FRegistryEntityHandle const handle) {
    auto& data{entity_buffers.current()};
    auto const index{find_index(handle)};
    assert(index != -1);
    data.healths[index] = 0;
    if (!std::ranges::contains(local_indices_to_remove, index)) {
        local_indices_to_remove.push_back(index);
    }
}
void Simulation::remove_dead_entities() {
    auto& data{entity_buffers.current()};
    ml::batch::sort_and_deduplicate_removal_indices(local_indices_to_remove);
    for (auto const index : local_indices_to_remove) {
        data.remove_at_swap(index, 1);
    }
    validate_array_sizes();
}

/* **************************************** */
// Combat
/* **************************************** */
void Simulation::handle_firing(TaskView const& data) {

    auto const n_ships{data.num()};
    auto const aim_threshold{fire_dot_product_threshold};
    auto const laser_damage{config.laser.damage};
    auto const laser_speed{config.laser.projectile_speed};
    auto const laser_max_distance{config.laser.max_distance};
    auto const laser_max_distance_sq{laser_max_distance * laser_max_distance};
    auto const desired_attack_distance{laser_max_distance *
                                       config.attack_distance_band.desired_ratio};
    auto const arrival_distance{config.arrival_distance};
    auto const attack_position_arrival_distance_sq{arrival_distance * arrival_distance};
    ml::simulation::fighters::FiringScratch scratch{&frame_memory_resource};
    auto& new_lasers{scratch.new_lasers};
    auto& can_fire{scratch.can_fire};
    auto& line_of_sight_starts{scratch.line_of_sight_starts};
    auto& line_of_sight_ends{scratch.line_of_sight_ends};
    auto& line_of_sight_results{scratch.line_of_sight_results};
    auto& firing_ignored_entities{scratch.ignored_entities};
    auto& firing_position_fighter_indices{scratch.position_fighter_indices};
    auto& firing_position_candidates{scratch.position_candidates};

    new_lasers.set_num(n_ships);
    firing_position_fighter_indices.reserve(n_ships);
    firing_position_candidates.reserve(n_ships);

    auto const fighter_count{static_cast<std::size_t>(n_ships)};
    auto const los_check_buffer{config.los_check_buffer};
    ml::simulation::fighters::prepare_firing(
        {.locations = data.locations.get_const_view(),
         .aim_directions = data.aim_directions.get_const_view(),
         .desired_aiming_directions = data.desired_aiming_directions.get_const_view(),
         .target_locations = data.target_locations.get_const_view(),
         .handles = {data.entity_handles.data(), fighter_count},
         .targets = {data.target_handles.data(), fighter_count},
         .target_distance_squared = {data.target_distance_sq.data(), fighter_count},
         .target_radii = {data.target_radii.data(), fighter_count},
         .attack_cooldowns = std::span<std::int16_t>{data.attack_cooldowns}},
        {.maximum_distance_squared = laser_max_distance_sq,
         .aim_threshold = aim_threshold,
         .fire_point_distance = fire_point_distance,
         .line_of_sight_buffer = los_check_buffer,
         .retry_cooldown = attack_retry_cooldown_tick_value},
        scratch);

    if (can_fire.is_empty()) {
        return;
    }

    auto const n_can_fire_before_los{can_fire.num()};
    line_of_sight_results.set_num(n_can_fire_before_los);

    spatial_query_manager.have_clear_lines(
        line_of_sight_starts.get_const_view(),
        line_of_sight_ends.get_const_view(),
        {line_of_sight_results.data(), static_cast<std::size_t>(line_of_sight_results.num())},
        {firing_ignored_entities.data(), static_cast<std::size_t>(firing_ignored_entities.num())});

    ml::simulation::fighters::resolve_firing_visibility(
        data.locations.get_const_view(),
        data.desired_move_locations.get_const_view(),
        std::span<std::int16_t>{data.attack_cooldowns},
        {line_of_sight_results.data(), static_cast<std::size_t>(n_can_fire_before_los)},
        attack_position_arrival_distance_sq,
        attack_retry_cooldown_tick_value,
        can_fire,
        firing_position_fighter_indices);

    auto const n_fire_point_candidates{ml::simulation::fighters::fire_point_candidate_count};
    for (std::uint32_t candidate_order{};
         candidate_order < n_fire_point_candidates && !firing_position_fighter_indices.is_empty();
         ++candidate_order) {
        auto const n_fighters{firing_position_fighter_indices.num()};
        line_of_sight_starts.set_num(n_fighters);
        line_of_sight_ends.set_num(n_fighters);
        line_of_sight_results.set_num(n_fighters);
        firing_ignored_entities.set_num(n_fighters);
        firing_position_candidates.set_num(n_fighters);

        for (std::int32_t i{}; i < n_fighters; ++i) {
            auto const ship_index{firing_position_fighter_indices[i]};
            firing_ignored_entities[i] = data.entity_handles[ship_index];
            auto const candidate{ml::simulation::fighters::make_fire_point_candidate(
                data.target_locations[ship_index],
                data.desired_move_locations[ship_index],
                fire_point_distance,
                los_check_buffer + data.target_radii[ship_index],
                desired_attack_distance,
                data.integral_biases[ship_index],
                data.float_biases[ship_index],
                candidate_order)};
            line_of_sight_starts.set(i, candidate.trace_start);
            line_of_sight_ends.set(i, candidate.trace_end);
            firing_position_candidates.set(i, candidate.location);
        }

        spatial_query_manager.have_clear_lines(
            line_of_sight_starts.get_const_view(),
            line_of_sight_ends.get_const_view(),
            {line_of_sight_results.data(), static_cast<std::size_t>(line_of_sight_results.num())},
            {firing_ignored_entities.data(),
             static_cast<std::size_t>(firing_ignored_entities.num())});

        ml::simulation::fighters::accept_visible_firing_positions(
            data.desired_move_locations,
            firing_position_candidates.get_const_view(),
            {line_of_sight_results.data(), static_cast<std::size_t>(n_fighters)},
            firing_position_fighter_indices);
    }

    auto const n_can_fire{can_fire.num()};
    new_lasers.set_num(n_can_fire);
    for (std::int32_t i{0}; i < n_can_fire; ++i) {
        auto const ship_index{can_fire[i]};
        auto const ship_location{data.locations[ship_index]};
        auto const direction{data.aim_directions[ship_index]};
        new_lasers.locations.set(i, ship_location + direction * fire_point_distance);
        new_lasers.rotations.set(i, ml::simulation::direction_to_rotation(direction));
        new_lasers.base_velocities.set(i, data.velocities[ship_index]);
        new_lasers.instigator_handles[i] = data.entity_handles[ship_index];
        new_lasers.sources[i] = {data.teams[ship_index],
                                 ml::simulation::EntityType::CapitalShipFighter};
        data.attack_cooldowns[ship_index] = attack_restart_ticks_;
    }

    new_lasers.set_damages(laser_damage);
    new_lasers.set_speeds(laser_speed);
    new_lasers.set_max_distances(laser_max_distance);
    laser_simulation.queue_laser_spawns(new_lasers.get_const_view());
}

/* **************************************** */
// Orders
/* **************************************** */
void Simulation::queue_orders(TestCapitalShipFighterOrderQueue const& queue) {
    order_queue.append_from(queue.get_const_view());
}
void Simulation::commit_orders() {

    auto& data{entity_buffers.current()};
    auto const n_orders{order_queue.num()};
    if (n_orders < 1) {
        return;
    }

    auto const count{static_cast<std::size_t>(data.num())};
    ml::simulation::fighters::apply_orders(
        {.handles = {data.entity_handles.data(), count},
         .tasks = {data.tasks.data(), count},
         .targets = {data.target_handles.data(), count},
         .locations = data.locations.get_const_view(),
         .desired_move_locations = data.desired_move_locations.get_view(),
         .attack_reposition_countdowns = std::span<std::int16_t>{data.attack_reposition_countdowns},
         .navigation = get_native_navigation_state()},
        order_queue.get_const_view(),
        {navigation_tick_periods.data(), static_cast<std::size_t>(navigation_tick_periods.size())},
        direct_movement_choice);
}

/* **************************************** */
// Misc
/* **************************************** */
void Simulation::clear_tick_buffers() {
    local_indices_to_remove.clear();
    entity_death_info.reset();
    spawn_queue.reset();
    order_queue.reset();
}

/* **************************************** */
// Checks
/* **************************************** */
#ifndef NDEBUG
void Simulation::validate_array_sizes() const {
    entity_buffers.current().validate_array_sizes();
}
void Simulation::check_fighter_tasks() const {

    auto current_task_group{Task::Standby};
    TaskSpans checked_task_spans{};
    auto const& data{entity_buffers.current()};
    auto const n_tasks{data.num()};
    for (std::int32_t i{}; i < n_tasks; ++i) {
        auto const task{data.tasks[i]};
        auto const task_value{std::to_underlying(task)};
        if (task == current_task_group) {
            ++checked_task_spans[task_value].count;
        } else if (task > current_task_group) {
            current_task_group = task;
            checked_task_spans[task_value].offset = i;
            checked_task_spans[task_value].count = 1;
        } else {
            ml::fatal_error(std::format("Found task {} when current group was {}",
                                        ml::simulation::to_string_view(task),
                                        ml::simulation::to_string_view(current_task_group)));
        }
    }

    for (std::size_t i{1}; i < n_task_types; ++i) {
        auto const last_span{checked_task_spans[i - 1]};
        auto const last_end{last_span.end()};
        auto& current_span{checked_task_spans[i]};
        if (current_span.offset < last_end) {
            current_span.offset = last_end;
            assert(current_span.count == 0);
        }
    }

    if (checked_task_spans != task_spans) {
        std::string message{"Incorrect task spans."};
        for (std::size_t i{}; i < n_task_types; ++i) {
            message += std::format("\\n    {}: expected ({}, {}), got ({}, {})",
                                   ml::simulation::to_string_view(static_cast<Task>(i)),
                                   task_spans[i].offset,
                                   task_spans[i].count,
                                   checked_task_spans[i].offset,
                                   checked_task_spans[i].count);
        }
        ml::fatal_error(message);
    }
}
#endif
} // namespace ml::test_capital_ship_fighters
