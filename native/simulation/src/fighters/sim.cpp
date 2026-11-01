#include "ioj/sim/fighters/sim.h"

#include <ioj/column_math.h>
#include <ioj/countdown.h>
#include <ioj/diagnostics.h>
#include <ioj/frame_array.h>
#include <ioj/generated/array_math_kernels.h>
#include <ioj/periodic_tick_countdown.h>
#include <ioj/projectile_intercept.h>
#include <ioj/rotator_math.h>
#include <ioj/sim/batch_operations.h>
#include <ioj/sim/collision/spatial_query_manager.h>
#include <ioj/sim/collision/trace_hits.h>
#include <ioj/sim/combat_events.h>
#include <ioj/sim/deterministic_bias.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/lasers/laser_source.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/phase_checks.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/tick_countdown.h>
#include <ioj/vector_math.h>
#include <ioj/vector_normalization.h>
#include <ioj/vector_operations.h>
#include <ioj/vectors3f.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <format>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace ioj::sim::fighters {

/* **************************************** */
// Navigation helpers
/* **************************************** */
auto Sim::get_navigation_tick_period(NavigationRiskTier const tier) const -> std::int16_t {
    auto const index{static_cast<std::uint32_t>(tier)};
    assert(index < navigation_tick_periods.size());
    return navigation_tick_periods[index];
}

/* **************************************** */
// Configuration
/* **************************************** */
// Level initialisation
void Sim::initialise(FighterSimConfig const& new_config, FighterLevelData const level_data) {
    config = new_config;
    auto const& clock{level_data.clock};

    for (auto& is_participant : participant_mask) {
        is_participant = 0;
    }
    for (auto const team : level_data.participating_teams) {
        auto const team_index{static_cast<std::uint32_t>(team)};
        if (team_index < participant_mask.size()) {
            participant_mask[team_index] = 1;
        }
    }

    assert(std::in_range<std::uint32_t>(level_data.participating_teams.size()));
    auto const participant_count{static_cast<std::uint32_t>(level_data.participating_teams.size())};
    per_team_limit =
        participant_count == 0 ? 0 : config.max_live_fighters.get() / participant_count;

    SANDBOX_PROFILE_SCOPE("fighters::Sim::initialise");
    profiling::plot("Sandbox/FighterCount", 0);
    profiling::plot("Sandbox/FightersAvoiding", 0);
    profiling::plot("Sandbox/FighterNavigationTraces", 0);
    profiling::plot("Sandbox/FightersSeparating", 0);
    profiling::plot("Sandbox/FighterSeparationQueries", 0);
    profiling::plot("Sandbox/FighterSeparationCandidates", 0);
    profiling::plot("Sandbox/FighterDenseDirectionSelections", 0);
    profiling::plot("Sandbox/FighterSteeringMemory", 0);
    profiling::plot("Sandbox/FighterNavigationClear", 0);
    profiling::plot("Sandbox/FighterNavigationNearby", 0);
    profiling::plot("Sandbox/FighterNavigationActive", 0);
    profiling::plot("Sandbox/FighterNavigationImmediate", 0);
    attack_cleaner_ = 0;
    assert(config.fire_point_distance >= 0.f);

    std::array<float, enum_count<NavigationRiskTier>()> const frequencies{
        config.avoidance_clear_update_frequency,
        config.avoidance_update_frequency,
        config.avoidance_active_update_frequency,
        config.avoidance_immediate_update_frequency,
    };
    auto const frequency_count{frequencies.size()};
    for (std::uint32_t i{}; static_cast<std::size_t>(i) < frequency_count; ++i) {
        navigation_tick_periods[i] =
            clock.frequency_to_tick_period<std::int16_t>(Frequency::make(frequencies[i]));
    }
    auto const clear_update_interval{static_cast<float>(
        get_navigation_tick_period(NavigationRiskTier::Clear) * clock.get_tick_period())};
    minimum_navigation_lookahead_time = clear_update_interval * 1.25f;

    attack_restart_ticks_ =
        clock.duration_to_tick_period<std::int16_t>(Duration::make(config.laser.fire_cooldown));

    attack_retry_cooldown_tick_value =
        clock.duration_to_tick_period<std::int16_t>(Duration::make(config.attack_retry_cooldown));
}
Sim::Sim(EntityTables& entity_tables) noexcept
    : entity_tables_{entity_tables} {}

/* **************************************** */
// Sim phases
/* **************************************** */
// Preparation
void Sim::commit_spawns(PreparationInputs const inputs) {
    assert(is_preparation_mutation_allowed(entity_tables_.lookups.clock()));
    SANDBOX_PROFILE_SCOPE("fighters::Sim::commit_spawns");

    auto const n_cur{get_num_instances()};
    auto const n_new{spawn_queue.num()};

    spawn_queue.get_const_view().validate();
    if (n_new < 1) {
        return;
    }

    entities_.add_defaulted(n_new);
    auto const data{entities_.get_view()};
    auto const new_data{data.get_view(n_cur, n_new)};
    auto const spawns{spawn_queue.get_const_view()};
    auto const navigation_period{get_navigation_tick_period(NavigationRiskTier::Nearby)};
    copy_vectors(new_data.view_locations().resolve(), spawns.view_locations().resolve());
    copy_vectors(new_data.view_desired_move_locations().resolve(),
                 spawns.view_locations().resolve());
    std::ranges::fill(new_data.speeds(), config.speed);
    std::ranges::copy(spawns.teams(), new_data.teams().begin());
    std::ranges::fill(new_data.navigation_risk_tiers(),
                      NavigationRiskCode{
                          static_cast<NavigationRiskCode::value_type>(NavigationRiskTier::Nearby)});
    std::ranges::fill(new_data.avoidance_choice_indices(),
                      AvoidanceChoice{AvoidanceChoice::DirectMovement});
    std::ranges::fill(new_data.navigation_update_countdowns_periods(), navigation_period);

    auto const aim_directions{new_data.view_aim_directions().resolve()};
    auto const rotations{spawns.view_rotations().resolve()};
    auto const pitches{rotations.pitches()};
    auto const yaws{rotations.yaws()};
    auto const rolls{rotations.rolls()};
    for (std::uint32_t index{}; index < n_new; ++index) {
        set_vector(aim_directions,
                   index,
                   forward_direction(Rotator3f{pitches[index], yaws[index], rolls[index]}));
    }

    auto const entity_ids{new_data.entity_ids()};
    auto const teams{new_data.teams()};
    inputs.ledger.record_spawns(EntityType::Fighter, teams, config.health, entity_ids);
    entity_tables_.health.initialise_entities<EntityType::Fighter>(entity_ids, config.health);
    make_deterministic_biases(
        std::span<EntityId const>{data.entity_ids()}.subspan(n_cur, n_new),
        std::span<std::uint32_t>{data.integral_biases()}.subspan(n_cur, n_new),
        std::span<float>{data.float_biases()}.subspan(n_cur, n_new));
}
void Sim::update_entity_lookup_table() {
    auto const rows{entities_.get_const_view()};
    entity_tables_.publish<EntityType::Fighter>(rows.entity_ids(), rows.teams(), config.health);
}
void Sim::prepare_tick() {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::prepare_tick");

    auto const data{entities_.get_view()};
    tick_periodic_countdowns<std::int16_t>(data.navigation_update_countdowns_remaining_ticks());
    clear_tick_buffers();

    for (auto& capacity : remaining_team_capacity) {
        capacity = per_team_limit;
    }
    for (auto const team : data.teams()) {
        auto const team_index{static_cast<std::uint32_t>(team)};
        if (team_index < participant_mask.size() && participant_mask[team_index] != 0) {
            remaining_team_capacity[team_index] =
                std::max(0, remaining_team_capacity[team_index] - 1);
        } else {
            log_error(
                std::format("Live fighter has invalid or non-participating team {}", team_index));
        }
    }
    tick_countdowns<std::int16_t>(data.attack_cooldowns(), attack_cleaner_, 16384);
    update_entity_lookup_table();
}

// Thinking
void Sim::think(ThinkingInputs const inputs) {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::think");
    refresh_target_data(inputs);
    plan_movement(inputs);
}
void Sim::plan_movement(ThinkingInputs const inputs) {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::plan_movement");
    auto const data{entities_.get_view()};
    auto const count{data.num()};
    if (count == 0) {
        return;
    }

    auto const locations{data.view_locations().resolve()};
    auto const destinations{data.view_desired_move_locations().resolve()};
    auto const movement_directions{data.view_movement_directions().resolve()};
    native_math::direction_and_distance(movement_directions.xs().data(),
                                        movement_directions.ys().data(),
                                        movement_directions.zs().data(),
                                        data.move_distances().data(),
                                        locations.xs().data(),
                                        locations.ys().data(),
                                        locations.zs().data(),
                                        destinations.xs().data(),
                                        destinations.ys().data(),
                                        destinations.zs().data(),
                                        count);
    update_navigation_steering(inputs);

    auto const target_locations{data.view_target_locations().resolve()};
    auto const target_velocities{data.view_target_velocities().resolve()};
    auto const intercept_times{data.intercept_times()};
    ::ioj::detail::solve_intercept_times_soa_loop::solve_intercept_times(
        intercept_times.data(),
        locations,
        target_locations,
        target_velocities,
        config.laser.projectile_speed);

    auto const dt{static_cast<float>(inputs.clock.get_tick_period())};
    auto const turn_fraction{std::min(1.f, config.turn_speed_unitless * dt)};
    auto const target_ids{data.target_ids()};
    auto const choices{data.avoidance_choice_indices()};
    auto const aim_directions{data.view_aim_directions().resolve()};
    auto const desired_aim{data.view_desired_aiming_directions().resolve()};
    auto const planned_aim{data.view_planned_aim_directions().resolve()};
    auto const move_distances{data.move_distances()};
    auto const speeds{data.speeds()};
    auto const target_distance_sq{data.target_distance_sq()};
    auto const target_distances{data.target_distances()};
    for (std::uint32_t index{}; index < count; ++index) {
        auto const current_aim{vector_at(aim_directions, index)};
        auto desired_direction{current_aim};
        if (target_ids[index].is_valid()) {
            auto const intercept{vector_at(target_locations, index) +
                                 vector_at(target_velocities, index) * intercept_times[index]};
            desired_direction = native_math::safe_normal(intercept - vector_at(locations, index));
        } else if (move_distances[index] > 0.f) {
            desired_direction = vector_at(movement_directions, index);
        }
        set_vector(desired_aim, index, desired_direction);
        if (is_avoidance_direction_choice(choices[index].value())) {
            desired_direction = vector_at(movement_directions, index);
        }
        set_vector(
            planned_aim, index, current_aim + (desired_direction - current_aim) * turn_fraction);
        move_distances[index] = std::min(move_distances[index], speeds[index] * dt);
        auto const next_location{vector_at(locations, index) +
                                 vector_at(movement_directions, index) * move_distances[index]};
        auto const distance_sq{size_squared(next_location - vector_at(target_locations, index))};
        target_distance_sq[index] = distance_sq;
        target_distances[index] = std::sqrt(distance_sq);
    }
}
void Sim::generate_fire_commands(ThinkingInputs const inputs) {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::generate_fire_commands");
    handle_firing(entities_.get_view(), inputs);
}

// Action
void Sim::apply_movement(ActionInputs const inputs) {
    auto* const scratch_resource{&inputs.scratch_resource};

    SANDBOX_PROFILE_SCOPE("fighters::Sim::apply_movement");
    auto const data{entities_.get_view()};
    overlap_candidates_.clear();
    auto const count{data.num()};
    auto const aim_directions{data.view_aim_directions().resolve()};
    auto const planned_directions{data.view_planned_aim_directions().resolve()};
    auto const entity_ids{data.entity_ids()};

    for (std::uint32_t index{}; index < count; ++index) {
        auto const direction{vector_at(aim_directions, index)};
        auto const planned_direction{vector_at(planned_directions, index)};
        if (direction.x() == planned_direction.x() && direction.y() == planned_direction.y() &&
            direction.z() == planned_direction.z()) {
            continue;
        }
        auto const before{direction_to_rotation(direction)};
        auto const after{direction_to_rotation(planned_direction)};
        if (before.pitch != after.pitch || before.yaw != after.yaw || before.roll != after.roll) {
            overlap_candidates_.push_back(entity_ids[index]);
        }
    }
    data.view_velocities().resolve().each_column(
        [](auto column) { std::ranges::fill(column, 0.f); });
    copy_vectors(aim_directions, planned_directions.get_const_view());
    move(inputs.dt, data, scratch_resource);
    std::ranges::sort(overlap_candidates_);
    auto const duplicates{std::ranges::unique(overlap_candidates_)};
    overlap_candidates_.erase(duplicates.begin(), duplicates.end());

    auto const fire_count{static_cast<std::uint32_t>(pending_fire_indices_.size())};
    auto const requests{inputs.laser_simulation.append_spawn_requests(fire_count)};
    auto const laser_locations{requests.view_locations().resolve()};
    auto const laser_rotations{requests.view_rotations().resolve()};
    auto const laser_velocities{requests.view_base_velocities().resolve()};
    auto const laser_ids{requests.instigator_ids()};
    auto const laser_sources{requests.sources()};
    std::ranges::fill(requests.damages(), config.laser.damage);
    std::ranges::fill(requests.speeds(), config.laser.projectile_speed);
    std::ranges::fill(requests.max_distances(), config.laser.max_distance);

    auto const locations{data.view_locations().resolve()};
    auto const velocities{data.view_velocities().resolve()};
    auto const teams{data.teams()};

    for (std::uint32_t row{}; row < fire_count; ++row) {
        auto const index{pending_fire_indices_[row]};
        auto const direction{vector_at(aim_directions, index)};
        set_vector(laser_locations,
                   row,
                   vector_at(locations, index) + direction * config.fire_point_distance);
        set_rotation(laser_rotations, row, direction_to_rotation(direction));
        set_vector(laser_velocities, row, vector_at(velocities, index));
        laser_ids[row] = entity_ids[index];
        laser_sources[row] = {teams[index], EntityType::Fighter};
    }
    pending_fire_indices_.clear();
}

// Resolution
void Sim::resolve_damage_events(ResolutionInputs const inputs) {
    auto* const scratch_resource{&inputs.scratch_resource};

    SANDBOX_PROFILE_SCOPE("fighters::Sim::resolve_damage_events");

    auto const data{entities_.get_view()};
    auto const healths{entity_tables_.health.get_view<EntityType::Fighter>()};
    auto const damage_events{inputs.combat_events.events_for(EntityType::Fighter)};
    sim::resolve_damage_events(damage_events,
                               entity_tables_.lookups.for_type(EntityType::Fighter),
                               data.entity_ids(),
                               data.view_locations().get_const_view().resolve(),
                               healths,
                               local_indices_to_remove,
                               entity_death_info,
                               inputs.ledger,
                               scratch_resource);
}
void Sim::publish_deaths(ResolutionInputs const inputs) {
    inputs.ledger.record_deaths(entity_death_info.get_const_view(), inputs.tick);
}

// ResolutionCommit
void Sim::remove_components() {
    assert(is_removal_allowed(entity_tables_.lookups.clock()));
    SANDBOX_PROFILE_SCOPE("fighters::Sim::remove_components");

    sort_and_deduplicate_removal_indices(local_indices_to_remove);
}
void Sim::remove_entities() {
    assert(is_removal_allowed(entity_tables_.lookups.clock()));
    SANDBOX_PROFILE_SCOPE("fighters::Sim::remove_entities");

    remove_dead_entities();
    local_indices_to_remove.clear();
    entity_death_info.reset();
}
void Sim::finish_action() {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::finish_action");
    profiling::plot("Sandbox/FighterCount", get_num_instances());
}

/* **************************************** */
// Movement
/* **************************************** */
void Sim::move(float const dt,
               EntityStorage::View fighters,
               FrameMemoryResource* const scratch_resource) {
    assert(dt > 0.f);
    auto const count{fighters.num()};
    auto const directions{fighters.view_movement_directions().get_const_view().resolve()};
    Vectors3f previous_locations{*scratch_resource};
    previous_locations.set_num(count);
    copy_vectors(previous_locations.get_view().resolve(),
                 fighters.view_locations().get_const_view().resolve());
    auto const move_distances{fighters.move_distances()};
    auto const velocities{fighters.view_velocities().resolve()};
    auto const locations{fighters.view_locations().resolve()};
    auto const entity_ids{fighters.entity_ids()};
    auto const direction_xs{directions.xs()};
    auto const direction_ys{directions.ys()};
    auto const direction_zs{directions.zs()};
    auto const velocity_xs{velocities.xs()};
    auto const velocity_ys{velocities.ys()};
    auto const velocity_zs{velocities.zs()};

    for (std::uint32_t index{}; index < count; ++index) {
        auto const move_distance{move_distances[index]};
        auto const velocity_scale{move_distance / dt};
        velocity_xs[index] = direction_xs[index] * velocity_scale;
        velocity_ys[index] = direction_ys[index] * velocity_scale;
        velocity_zs[index] = direction_zs[index] * velocity_scale;
    }
    native_math::add_scaled_product_in_place(locations.xs().data(),
                                             locations.ys().data(),
                                             locations.zs().data(),
                                             directions.xs().data(),
                                             directions.ys().data(),
                                             directions.zs().data(),
                                             move_distances.data(),
                                             1.f,
                                             count);
    auto const before_locations{previous_locations.get_const_view().resolve()};
    for (std::uint32_t index{}; index < count; ++index) {
        auto const before{before_locations[index]};
        auto const after{vector_at(locations, index)};
        if (before.x() != after.x() || before.y() != after.y() || before.z() != after.z()) {
            overlap_candidates_.push_back(entity_ids[index]);
        }
    }
}
void Sim::update_navigation_steering(ThinkingInputs const inputs) {
    auto* const scratch_resource{&inputs.scratch_resource};

    SANDBOX_PROFILE_SCOPE("fighters::Sim::update_navigation_steering");

    auto const clearance{inputs.collision_radius + config.avoidance_clearance_buffer};
    auto const minimum_lookahead_distance{inputs.collision_radius * 2.f};
    auto const avoidance_lookahead_time{
        std::max(config.avoidance_lookahead_time.get(), minimum_navigation_lookahead_time)};
    auto const active_update_interval{static_cast<float>(
        get_navigation_tick_period(NavigationRiskTier::Active) * inputs.clock.get_tick_period())};
    auto const safe_progress_time{active_update_interval * 1.25f};
    navigation_telemetry = {};
    NavigationScratch scratch{scratch_resource};

    // Only expired countdowns observe the world. Held steering is applied to every mover.
    collect_navigation_updates(scratch);
    update_separation_observations(scratch, inputs);
    apply_separation_steering();

    // Hard sweeps have final authority over the traffic-biased preferred direction.
    scan_preferred_navigation(
        scratch, clearance, avoidance_lookahead_time, minimum_lookahead_distance, inputs);
    scan_alternative_navigation(
        scratch, clearance, avoidance_lookahead_time, minimum_lookahead_distance, inputs);
    select_navigation_alternatives(scratch, safe_progress_time);
    apply_navigation_choices(scratch);
    publish_navigation_telemetry();
}
void Sim::collect_navigation_updates(NavigationScratch& scratch) {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::collect_navigation_updates");

    auto const data{entities_.get_view()};
    PeriodicTickCountdownView<std::int16_t> const countdowns{
        data.navigation_update_countdowns_remaining_ticks(),
        data.navigation_update_countdowns_periods()};
    auto const count{static_cast<std::uint32_t>(countdowns.num())};
    scratch.ready_fighter_indices.reserve(count);
    scratch.observed_risk_tiers.set_num(count);

    auto const move_distances{data.move_distances()};
    auto const fighter_count{data.num()};
    for (std::uint32_t index{}; index < fighter_count; ++index) {
        if (move_distances[index] <= 0.f) {
            continue;
        }

        auto const element{static_cast<std::size_t>(index)};
        if (!countdowns.try_consume(element)) {
            continue;
        }

        scratch.ready_fighter_indices.add(index);
        scratch.observed_risk_tiers[index] = static_cast<std::uint8_t>(NavigationRiskTier::Clear);
    }
}
void Sim::update_separation_observations(NavigationScratch& scratch, ThinkingInputs const inputs) {
    auto* const scratch_resource{&inputs.scratch_resource};

    SANDBOX_PROFILE_SCOPE("fighters::Sim::update_separation_observations");

    auto const data{entities_.get_view()};
    // Fixed capacity bounds scoring work and keeps neighbour storage off the heap.
    std::array<EntityId, max_separation_neighbours> nearby_fighters;
    std::array<SeparationNeighbour, max_separation_neighbours> neighbours;
    std::array<EntityInstanceHandle, max_separation_neighbours> nearby_handles;
    auto const& fighter_lookup{entity_tables_.lookups.for_type(EntityType::Fighter)};
    auto const separation_radius{config.separation_radius};
    auto const immediate_distance{inputs.collision_radius * 2.f};
    auto const close_distance{std::max(separation_radius * 0.5f, immediate_distance)};
    auto const immediate_distance_sq{immediate_distance * immediate_distance};
    auto const close_distance_sq{close_distance * close_distance};

    auto const movement_directions{data.view_movement_directions().resolve()};
    auto const move_distances{data.move_distances()};
    auto const separation_steering{data.view_separation_steering().resolve()};
    auto const choices{data.avoidance_choice_indices()};
    auto const clear_scan_counts{data.avoidance_clear_scan_counts()};
    auto const locations{data.view_locations().resolve()};
    auto const entity_ids{data.entity_ids()};
    auto const risk_tiers{data.navigation_risk_tiers()};
    auto const integral_biases{data.integral_biases()};
    auto const float_biases{data.float_biases()};

    for (auto const fighter_index : scratch.ready_fighter_indices) {
        auto const goal_direction{vector_at(movement_directions, fighter_index)};
        auto const move_distance{move_distances[fighter_index]};
        if (native_math::is_nearly_zero(
                goal_direction.x(), goal_direction.y(), goal_direction.z(), 1.e-4f) ||
            move_distance <= 0.f) {
            set_vector(separation_steering, fighter_index, Vector3f{});
            choices[fighter_index] = AvoidanceChoice{AvoidanceChoice::DirectMovement};
            clear_scan_counts[fighter_index] = NavigationScanCount{};
            continue;
        }

        auto const fighter_location{vector_at(locations, fighter_index)};
        auto const fighter_id{entity_ids[fighter_index]};
        auto const n_nearby{
            inputs.spatial_query_manager.collect_entities_of_type_in_range(fighter_location,
                                                                           EntityType::Fighter,
                                                                           separation_radius,
                                                                           fighter_id,
                                                                           nearby_fighters,
                                                                           scratch_resource)};
        ++navigation_telemetry.separation_query_count;
        navigation_telemetry.separation_candidate_count += n_nearby;

        // Bind the varying result count returned by this fighter's scan.
        // NOLINTBEGIN(ioj-loop-view-construction,ioj-loop-view-accessor-call)
        fighter_lookup.lookup_handles(std::span{nearby_fighters}.first(n_nearby),
                                      std::span{nearby_handles}.first(n_nearby));
        // NOLINTEND(ioj-loop-view-construction,ioj-loop-view-accessor-call)
        for (std::uint32_t index{}; index < n_nearby; ++index) {
            auto const local_index{nearby_handles[index].index()};
            neighbours[index] = {entity_ids[local_index], vector_at(locations, local_index)};
        }

        auto const previous_memory{vector_at(separation_steering, fighter_index)};
        auto const current_tier{static_cast<NavigationRiskTier>(risk_tiers[fighter_index].value())};
        auto const elapsed_since_scan{static_cast<float>(get_navigation_tick_period(current_tier)) *
                                      inputs.clock.get_tick_period()};
        auto const memory_retention{
            config.steering_memory_duration > 0.f
                ? static_cast<float>(std::clamp(
                      1.0 - elapsed_since_scan / config.steering_memory_duration, 0.0, 1.0))
                : 0.f};
        auto const observation{fighters::observe_separation(
            fighter_location,
            entity_ids[fighter_index],
            goal_direction,
            previous_memory,
            // Each scan produces its own neighbour set.
            // NOLINTNEXTLINE(ioj-loop-view-construction)
            {neighbours.data(), static_cast<std::size_t>(n_nearby)},
            {
                .separation_radius = separation_radius,
                .immediate_distance_squared = immediate_distance_sq,
                .close_distance_squared = close_distance_sq,
                .memory_retention = static_cast<float>(memory_retention),
                .separation_strength = config.separation_strength,
                .dense_traffic_neighbour_threshold = config.dense_traffic_neighbour_threshold,
                .integral_bias = integral_biases[fighter_index],
                .float_bias = float_biases[fighter_index],
            })};
        if (observation.dense_direction_selected) {
            ++navigation_telemetry.dense_direction_selection_count;
        }
        set_vector(separation_steering, fighter_index, observation.steering_memory);
        scratch.observed_risk_tiers[fighter_index] =
            static_cast<std::uint8_t>(observation.risk_tier);
    }
}
void Sim::apply_separation_steering() {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::apply_separation_steering");

    auto const data{entities_.get_view()};
    auto const move_distances{data.move_distances()};
    auto const movement_directions{data.view_movement_directions().resolve()};
    auto const separation_steering{data.view_separation_steering().resolve()};

    auto const fighter_count{data.num()};
    for (std::uint32_t index{}; index < fighter_count; ++index) {
        if (move_distances[index] <= 0.f) {
            continue;
        }

        auto const direction{
            make_separation_steering_direction(vector_at(movement_directions, index),
                                               vector_at(separation_steering, index),
                                               config.separation_strength)};
        if (direction) {
            set_vector(movement_directions, index, *direction);
        }
    }
}
void Sim::scan_preferred_navigation(NavigationScratch& scratch,
                                    float const clearance,
                                    float const avoidance_lookahead_time,
                                    float const minimum_lookahead_distance,
                                    ThinkingInputs const inputs) {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::scan_preferred_navigation");

    auto const data{entities_.get_view()};
    auto const count{scratch.ready_fighter_indices.num()};
    scratch.line_of_sight_starts.reserve(count);
    scratch.line_of_sight_ends.reserve(count);
    scratch.trace_fighter_indices.reserve(count);
    scratch.blocked_fighter_indices.reserve(count);

    auto const movement_directions{data.view_movement_directions().resolve()};
    auto const move_distances{data.move_distances()};
    auto const speeds{data.speeds()};
    auto const locations{data.view_locations().resolve()};

    for (auto const index : scratch.ready_fighter_indices) {
        auto const element{static_cast<std::size_t>(index)};
        auto const direction{vector_at(movement_directions, index)};
        auto const move_distance{move_distances[element]};
        if (native_math::is_nearly_zero(direction.x(), direction.y(), direction.z(), 1.e-4f) ||
            move_distance <= 0.f) {
            continue;
        }

        auto const speed_distance{speeds[element] * avoidance_lookahead_time};
        auto const requested_distance{std::max(speed_distance, minimum_lookahead_distance)};
        auto const distance{std::min(move_distance, requested_distance)};
        auto const start{vector_at(locations, index)};
        scratch.line_of_sight_starts.add(start);
        scratch.line_of_sight_ends.add(start + direction * distance);
        scratch.trace_fighter_indices.add(index);
    }

    auto const clear_scan_counts{data.avoidance_clear_scan_counts()};
    auto const choices{data.avoidance_choice_indices()};

    auto const n_direct_traces{scratch.trace_fighter_indices.num()};
    if (n_direct_traces > 0) {
        execute_navigation_sweeps(scratch, clearance, inputs);
        auto const hit_flags{scratch.trace_hits.get_const_view().hits()};

        for (std::uint32_t index{}; index < n_direct_traces; ++index) {
            auto const fighter_index{scratch.trace_fighter_indices[index]};
            auto const element{static_cast<std::size_t>(fighter_index)};
            if (scratch.line_of_sight_results[index].value() == 0 ||
                hit_flags[index].value() != 0) {
                scratch.blocked_fighter_indices.add(fighter_index);
                clear_scan_counts[element] = NavigationScanCount{};
                continue;
            }
            if (choices[element].value() == AvoidanceChoice::DirectMovement) {
                clear_scan_counts[element] = NavigationScanCount{};
                continue;
            }

            auto& clear_count{clear_scan_counts[element]};
            clear_count.set_value(
                static_cast<NavigationScanCount::value_type>(clear_count.value() + 1));
            if (clear_count.value() >= clear_scans_to_end_avoidance) {
                choices[element] = AvoidanceChoice{AvoidanceChoice::DirectMovement};
                clear_count = NavigationScanCount{};
            }
        }
    }
}
void Sim::scan_alternative_navigation(NavigationScratch& scratch,
                                      float const clearance,
                                      float const avoidance_lookahead_time,
                                      float const minimum_lookahead_distance,
                                      ThinkingInputs const inputs) {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::scan_alternative_navigation");

    auto const data{entities_.get_const_view()};
    scratch.trace_fighter_indices.clear();
    scratch.trace_choice_indices.clear();
    scratch.line_of_sight_starts.reset();
    scratch.line_of_sight_ends.reset();
    scratch.line_of_sight_results.clear();
    scratch.trace_hits.reset();

    auto const count{scratch.blocked_fighter_indices.num() * n_avoidance_choices};
    scratch.line_of_sight_starts.reserve(count);
    scratch.line_of_sight_ends.reserve(count);
    scratch.trace_choice_indices.reserve(count);
    auto const speeds{data.speeds()};
    auto const move_distances{data.move_distances()};
    auto const locations{data.view_locations().resolve()};
    auto const movement_directions{data.view_movement_directions().resolve()};
    auto const float_biases{data.float_biases()};
    auto const integral_biases{data.integral_biases()};
    auto const choices{data.avoidance_choice_indices()};

    for (auto const index : scratch.blocked_fighter_indices) {
        auto const element{static_cast<std::size_t>(index)};
        auto const speed_distance{speeds[element] * avoidance_lookahead_time};
        auto const requested_distance{std::max(speed_distance, minimum_lookahead_distance)};
        auto const lookahead_distance{std::min(move_distances[element], requested_distance)};
        auto const start{vector_at(locations, index)};
        auto const frame{
            make_avoidance_frame(vector_at(movement_directions, index), float_biases[element])};
        std::array<Vector3f, n_avoidance_choices> directions;
        make_avoidance_directions(frame, directions);
        std::array<std::int8_t, n_avoidance_choices> order;
        make_avoidance_choice_order(integral_biases[element], choices[element].value(), order);

        for (auto const choice : order) {
            auto const direction{directions[static_cast<std::size_t>(choice)]};
            scratch.line_of_sight_starts.add(start);
            scratch.line_of_sight_ends.add(start + direction * lookahead_distance);
            scratch.trace_choice_indices.add(choice);
        }
    }
    assert(scratch.line_of_sight_ends.num() == count);
    execute_navigation_sweeps(scratch, clearance, inputs);
}
void Sim::execute_navigation_sweeps(NavigationScratch& scratch,
                                    float const clearance,
                                    ThinkingInputs const inputs) {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::execute_navigation_sweeps");

    auto const trace_count{scratch.line_of_sight_ends.num()};
    if (trace_count == 0) {
        return;
    }
    auto const moving_half_extent{Vector3f{clearance, clearance, clearance}};
    scratch.line_of_sight_results.set_num(trace_count);
    inputs.spatial_query_manager.are_spheres_in_bounds(
        scratch.line_of_sight_ends.get_const_view().resolve(),
        clearance,
        {scratch.line_of_sight_results.data(),
         static_cast<std::size_t>(scratch.line_of_sight_results.num())});
    scratch.trace_hits.set_num(trace_count);
    // Fighters contribute soft steering; solid entities (including the parent capital) block.
    inputs.spatial_query_manager.sweep_closest_aabbs(
        scratch.line_of_sight_starts.get_const_view().resolve(),
        scratch.line_of_sight_ends.get_const_view().resolve(),
        moving_half_extent,
        scratch.trace_hits.get_view(),
        {},
        collision::TraceEntityFilter::ExcludeFighters);
    navigation_telemetry.hard_trace_count += trace_count;
}
void Sim::select_navigation_alternatives(NavigationScratch& scratch,
                                         float const safe_progress_time) {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::select_navigation_alternatives");

    auto const data{entities_.get_view()};
    auto const n_blocked_fighters{scratch.blocked_fighter_indices.num()};
    auto const locations{data.view_locations().resolve()};
    auto const speeds{data.speeds()};
    auto const choices{data.avoidance_choice_indices()};
    auto const trace_results{scratch.trace_hits.get_const_view()};
    auto const hits{trace_results.hits()};

    auto const hit_locations{trace_results.view_locations().resolve()};

    for (std::uint32_t blocked_index{}; blocked_index < n_blocked_fighters; ++blocked_index) {
        auto const fighter_index{scratch.blocked_fighter_indices[blocked_index]};
        auto const fighter_location{vector_at(locations, fighter_index)};
        // Partial progress must leave room until the next active scan, including its margin.
        auto const safe_progress_distance{speeds[fighter_index] * safe_progress_time};

        auto const candidate_begin{blocked_index * n_avoidance_choices};

        auto const candidate_count{static_cast<std::size_t>(n_avoidance_choices)};
        // Each fighter owns a distinct candidate subrange.
        // NOLINTBEGIN(ioj-loop-view-construction,ioj-loop-view-accessor-call)
        auto const chosen_choice{fighters::choose_navigation_alternative(
            fighter_location,
            safe_progress_distance,
            {scratch.trace_choice_indices.data() + candidate_begin, candidate_count},
            {scratch.line_of_sight_results.data() + candidate_begin, candidate_count},
            hits.subspan(candidate_begin, candidate_count),
            hit_locations.slice(candidate_begin, n_avoidance_choices),
            AvoidanceChoice::StopMovement)};
        // NOLINTEND(ioj-loop-view-construction,ioj-loop-view-accessor-call)

        choices[fighter_index] = AvoidanceChoice{chosen_choice};
    }
}
void Sim::apply_navigation_choices(NavigationScratch const& scratch) {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::apply_navigation_choices");

    auto const data{entities_.get_view()};
    auto const choices{data.avoidance_choice_indices()};
    auto const risk_tiers{data.navigation_risk_tiers()};
    auto const lower_risk_scan_counts{data.navigation_lower_risk_scan_counts()};
    auto const countdown_periods{data.navigation_update_countdowns_periods()};
    auto const remaining_ticks{data.navigation_update_countdowns_remaining_ticks()};

    for (auto const index : scratch.ready_fighter_indices) {
        auto const element{static_cast<std::size_t>(index)};
        auto observed{static_cast<NavigationRiskTier>(scratch.observed_risk_tiers[index])};
        if (choices[element].value() != AvoidanceChoice::DirectMovement) {
            observed = std::max(observed, NavigationRiskTier::Active);
        }
        auto const update{
            update_navigation_risk(static_cast<NavigationRiskTier>(risk_tiers[element].value()),
                                   observed,
                                   lower_risk_scan_counts[element].value(),
                                   lower_risk_scans_to_demote)};
        risk_tiers[element] =
            NavigationRiskCode{static_cast<NavigationRiskCode::value_type>(update.tier)};
        lower_risk_scan_counts[element] = NavigationScanCount{update.lower_risk_scan_count};
        auto const period{get_navigation_tick_period(update.tier)};
        countdown_periods[element] = period;
        remaining_ticks[element] = period;
    }

    auto const separation_steering{data.view_separation_steering().resolve()};
    auto const movement_directions{data.view_movement_directions().resolve()};
    auto const float_biases{data.float_biases()};

    auto const move_distances{data.move_distances()};
    auto const fighter_count{data.num()};
    for (std::uint32_t index{}; index < fighter_count; ++index) {
        if (move_distances[index] <= 0.f) {
            continue;
        }

        auto const element{static_cast<std::size_t>(index)};
        auto const steering{vector_at(separation_steering, index)};
        if (!native_math::is_nearly_zero(steering.x(), steering.y(), steering.z(), 1.e-4f)) {
            ++navigation_telemetry.separating_fighter_count;
            ++navigation_telemetry.steering_memory_fighter_count;
        }
        auto const choice{choices[element].value()};
        if (is_avoidance_direction_choice(choice)) {
            auto const frame{
                make_avoidance_frame(vector_at(movement_directions, index), float_biases[element])};
            set_vector(movement_directions, index, make_avoidance_direction(frame, choice));
            ++navigation_telemetry.avoiding_fighter_count;
        } else if (choice == AvoidanceChoice::StopMovement) {
            set_vector(movement_directions, index, Vector3f{0.f, 0.f, 0.f});
            ++navigation_telemetry.avoiding_fighter_count;
        }

        switch (static_cast<NavigationRiskTier>(risk_tiers[element].value())) {
            case NavigationRiskTier::Clear:
                ++navigation_telemetry.clear_risk_count;
                break;
            case NavigationRiskTier::Nearby:
                ++navigation_telemetry.nearby_risk_count;
                break;
            case NavigationRiskTier::Active:
                ++navigation_telemetry.active_risk_count;
                break;
            case NavigationRiskTier::Immediate:
                ++navigation_telemetry.immediate_risk_count;
                break;
            default:
                assert(false);
                break;
        }
    }
}
void Sim::publish_navigation_telemetry() const {
    profiling::plot("Sandbox/FightersAvoiding", navigation_telemetry.avoiding_fighter_count);
    profiling::plot("Sandbox/FighterNavigationTraces", navigation_telemetry.hard_trace_count);
    profiling::plot("Sandbox/FightersSeparating", navigation_telemetry.separating_fighter_count);
    profiling::plot("Sandbox/FighterSeparationQueries",
                    navigation_telemetry.separation_query_count);
    profiling::plot("Sandbox/FighterSeparationCandidates",
                    navigation_telemetry.separation_candidate_count);
    profiling::plot("Sandbox/FighterDenseDirectionSelections",
                    navigation_telemetry.dense_direction_selection_count);
    profiling::plot("Sandbox/FighterSteeringMemory",
                    navigation_telemetry.steering_memory_fighter_count);
    profiling::plot("Sandbox/FighterNavigationClear", navigation_telemetry.clear_risk_count);
    profiling::plot("Sandbox/FighterNavigationNearby", navigation_telemetry.nearby_risk_count);
    profiling::plot("Sandbox/FighterNavigationActive", navigation_telemetry.active_risk_count);
    profiling::plot("Sandbox/FighterNavigationImmediate",
                    navigation_telemetry.immediate_risk_count);
}

/* **************************************** */
// Accessors
/* **************************************** */
auto Sim::get_num_instances() const noexcept -> std::uint32_t {
    return entities_.num();
}

/* **************************************** */
// Targets
/* **************************************** */
void Sim::refresh_target_data(ThinkingInputs const inputs) {
    auto* const scratch_resource{&inputs.scratch_resource};

    auto const data{entities_.get_view()};
    auto const count{data.num()};
    auto const target_ids{data.target_ids()};
    auto const target_radii{data.target_radii()};
    FrameArray<EntityInstanceHandle> targets{scratch_resource};
    targets.set_num(count);
    inputs.spatial_query_manager.copy_entity_motion(target_ids,
                                                    data.view_target_locations().resolve(),
                                                    data.view_target_velocities().resolve(),
                                                    targets,
                                                    scratch_resource);
    for (std::uint32_t index{}; index < count; ++index) {
        if (!targets[index].is_valid()) {
            target_ids[index] = {};
        }
        auto const id{target_ids[index]};
        target_radii[index] =
            id.is_valid() ? inputs.spatial_query_manager.get_entity_type_radius(id.entity_type())
                          : 0.f;
    }
    distance_and_squared(data.target_distances(),
                         data.target_distance_sq(),
                         data.view_locations().get_const_view().resolve(),
                         data.view_target_locations().get_const_view().resolve());
}

/* **************************************** */
// Spawning
/* **************************************** */
auto Sim::accept_spawn_count(std::span<Team const> const teams) -> std::uint32_t {
    if (teams.empty()) {
        return 0;
    }
    auto const team{teams.front()};
    auto const team_index{static_cast<std::size_t>(team)};
    if (team_index >= participant_mask.size() || participant_mask[team_index] == 0) {
        log_error(
            std::format("Rejected fighter spawn request for invalid or non-participating team {}",
                        static_cast<std::uint32_t>(teams[0])));
        return 0;
    }

    for (auto const queued_team : teams) {
        if (queued_team != team) {
            log_error("Rejected fighter spawn wave containing multiple teams");
            return 0;
        }
    }

    auto& capacity{remaining_team_capacity[team_index]};
    assert(capacity >= 0);
    auto const accepted_count{
        std::min(static_cast<std::uint32_t>(teams.size()), static_cast<std::uint32_t>(capacity))};
    capacity -= static_cast<std::int32_t>(accepted_count);
    return accepted_count;
}
auto Sim::queue_spawns(FighterSpawnQueue::ConstView const new_spawns) -> std::uint32_t {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::queue_spawns");
    new_spawns.validate();
    auto const count{accept_spawn_count(new_spawns.teams())};
    spawn_queue.append_from(new_spawns, 0, count);
    return count;
}

/* **************************************** */
// Destruction
/* **************************************** */
void Sim::remove_dead_entities() {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::remove_dead_entities");
    auto& data{entities_};
    auto const columns{data.get_const_view()};
    auto const entity_ids{columns.entity_ids()};

    entity_tables_.lookups.for_type(EntityType::Fighter)
        .retire_rows(entity_ids, local_indices_to_remove);
    data.remove_at_swap(local_indices_to_remove);
}

/* **************************************** */
// Combat
/* **************************************** */
void Sim::handle_firing(EntityStorage::View data, ThinkingInputs const inputs) {
    auto* const scratch_resource{&inputs.scratch_resource};

    SANDBOX_PROFILE_SCOPE("fighters::Sim::handle_firing");

    auto const locations{data.view_locations().resolve()};
    auto const movement_directions{data.view_movement_directions().resolve()};
    auto const move_distances{data.move_distances()};

    auto predicted_location =
        [locations, movement_directions, move_distances](std::uint32_t const index) {
            return vector_at(locations, index) +
                   vector_at(movement_directions, index) * move_distances[index];
        };

    auto const n_ships{data.num()};
    auto const aim_threshold{config.fire_dot_product_threshold};
    auto const laser_max_distance{config.laser.max_distance};
    auto const laser_max_distance_sq{laser_max_distance * laser_max_distance};
    FrameArray<float> aiming_dot_products{scratch_resource};
    FrameArray<std::uint32_t> can_fire{scratch_resource};
    Vectors3f line_of_sight_starts{*scratch_resource};
    Vectors3f line_of_sight_ends{*scratch_resource};
    FrameArray<LineQueryResult> line_of_sight_results{scratch_resource};
    FrameArray<EntityId> firing_ignored_entities{scratch_resource};

    auto const los_check_buffer{config.los_check_buffer};
    TickCountdownView<std::int16_t> const cooldowns{
        std::span<std::int16_t>{data.attack_cooldowns()}, attack_retry_cooldown_tick_value};
    aiming_dot_products.set_num(n_ships);
    auto const planned_aim_directions{data.view_planned_aim_directions().resolve()};
    auto const desired_aiming_directions{data.view_desired_aiming_directions().resolve()};
    native_math::dot_product_vector(aiming_dot_products.data(),
                                    planned_aim_directions.xs().data(),
                                    planned_aim_directions.ys().data(),
                                    planned_aim_directions.zs().data(),
                                    desired_aiming_directions.xs().data(),
                                    desired_aiming_directions.ys().data(),
                                    desired_aiming_directions.zs().data(),
                                    n_ships);

    auto const target_distance_sq{data.target_distance_sq()};
    auto const target_ids{data.target_ids()};

    can_fire.reserve(n_ships);
    for (std::uint32_t index{}; index < n_ships; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        if (!cooldowns.is_ready(element)) {
            continue;
        }
        if (target_distance_sq[element] > laser_max_distance_sq ||
            !target_ids[element].is_valid() || aiming_dot_products[index] < aim_threshold) {
            cooldowns.restart_counter(element);
            continue;
        }

        can_fire.add(index);
    }

    auto const planned_directions{data.view_planned_aim_directions().resolve()};
    auto const target_radii{data.target_radii()};
    auto const entity_ids{data.entity_ids()};
    auto const target_locations{data.view_target_locations().resolve()};

    auto const firing_count{can_fire.num()};
    line_of_sight_starts.set_num(firing_count);
    line_of_sight_ends.set_num(firing_count);
    firing_ignored_entities.set_num(firing_count);
    for (std::uint32_t index{}; index < firing_count; ++index) {
        auto const fighter_index{can_fire[index]};
        auto const element{static_cast<std::size_t>(fighter_index)};
        auto const direction{vector_at(planned_directions, fighter_index)};
        auto const end_offset{los_check_buffer + target_radii[element]};
        firing_ignored_entities[index] = entity_ids[element];
        line_of_sight_starts.set(
            index, predicted_location(fighter_index) + direction * config.fire_point_distance);
        line_of_sight_ends.set(index,
                               vector_at(target_locations, fighter_index) - direction * end_offset);
    }

    if (can_fire.is_empty()) {
        return;
    }

    auto const n_can_fire_before_los{can_fire.num()};
    line_of_sight_results.set_num(n_can_fire_before_los);

    inputs.spatial_query_manager.have_clear_lines(
        line_of_sight_starts.get_const_view().resolve(),
        line_of_sight_ends.get_const_view().resolve(),
        {line_of_sight_results.data(), static_cast<std::size_t>(line_of_sight_results.num())},
        scratch_resource,
        {firing_ignored_entities.data(), static_cast<std::size_t>(firing_ignored_entities.num())});

    for (auto remaining{can_fire.num()}; remaining > 0; --remaining) {
        auto const index{remaining - 1};
        if (line_of_sight_results[index].value() == 0) {
            cooldowns.restart_counter(can_fire[index]);
            can_fire.remove_at_swap(index);
        }
    }

    auto const attack_cooldowns{data.attack_cooldowns()};

    auto const n_can_fire{can_fire.num()};
    for (std::uint32_t i{0}; i < n_can_fire; ++i) {
        auto const ship_index{can_fire[i]};
        pending_fire_indices_.push_back(ship_index);
        attack_cooldowns[ship_index] = attack_restart_ticks_;
    }
}

/* **************************************** */
// Misc
/* **************************************** */
void Sim::clear_tick_buffers() {
    local_indices_to_remove.clear();
    entity_death_info.reset();
    spawn_queue.reset();
}

} // namespace fighters
