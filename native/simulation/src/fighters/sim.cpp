#include "ioj/sim/fighters/sim.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <format>
#include <ioj/sim/column_math.h>
#include <ioj/sim/combat_events.h>
#include <ioj/sim/deterministic_bias.h>
#include <ioj/sim/fighter_frame_spawn_queue.h>
#include <ioj/sim/rotator_math.h>
#include <ioj/sim/vector_operations.h>
#include <limits>
#include <sandbox/core/countdown.h>
#include <sandbox/core/diagnostics.h>
#include <sandbox/core/periodic_tick_countdown.h>
#include <sandbox/core/tick_countdown.h>
#include <sandbox/core/vector_math.h>
#include <sandbox/core/vector_normalization.h>
#include <span>
#include <utility>
#include <vector>

#include <ioj/sim/batch_operations.h>
#include <ioj/sim/entity_ledger.h>
#include <ioj/sim/fighter_diagnostics.h>
#include <ioj/sim/frame_laser_spawn_requests.h>
#include <ioj/sim/frame_trace_hits.h>
#include <ioj/sim/frame_vectors3f.h>
#include <ioj/sim/laser_source.h>
#include <ioj/sim/lasers/frame_scratch.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/spatial_query_manager.h>

#include <sandbox/core/frame_array.h>
#include <sandbox/core/generated/array_math_kernels.h>
#include <sandbox/core/projectile_intercept.h>

namespace ioj::sim::fighters {

namespace firing_detail {
struct FirePointCandidate {
    Vector3f location;
    Vector3f trace_start;
    Vector3f trace_end;
};

struct FirePointAngleOffset {
    float yaw;
    float pitch;
};

inline constexpr std::array<FirePointAngleOffset, 16> angle_offsets{{
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

auto make_fire_point_candidate(Vector3f const target_location,
                               Vector3f const reference_location,
                               float const fire_point_distance,
                               float const trace_end_offset,
                               float const desired_attack_distance,
                               std::uint32_t const integral_bias,
                               float const float_bias,
                               std::uint32_t const candidate_order) noexcept -> FirePointCandidate {
    auto const base_direction{ml::native_math::safe_normal(reference_location - target_location)};
    auto rotation{direction_to_rotation(base_direction)};
    auto const first_index{integral_bias % angle_offsets.size()};
    auto const offset{angle_offsets[(first_index + candidate_order) % angle_offsets.size()]};
    rotation.yaw += float_bias * 360.f + offset.yaw;
    rotation.pitch += offset.pitch;
    auto const candidate_location{target_location +
                                  forward_direction(rotation) * desired_attack_distance};

    std::array<double, 3> const target{target_location.X, target_location.Y, target_location.Z};
    std::array<double, 3> const candidate{
        candidate_location.X, candidate_location.Y, candidate_location.Z};
    std::array<double, 3> aim{
        target[0] - candidate[0], target[1] - candidate[1], target[2] - candidate[2]};
    ml::native_math::safe_normal_components(aim[0], aim[1], aim[2]);
    std::array<double, 3> const start{candidate[0] + aim[0] * fire_point_distance,
                                      candidate[1] + aim[1] * fire_point_distance,
                                      candidate[2] + aim[2] * fire_point_distance};
    std::array<double, 3> direction{
        target[0] - start[0], target[1] - start[1], target[2] - start[2]};
    ml::native_math::safe_normal_components(direction[0], direction[1], direction[2]);
    return {candidate_location,
            HMM_V3(static_cast<float>(start[0]),
                   static_cast<float>(start[1]),
                   static_cast<float>(start[2])),
            HMM_V3(static_cast<float>(target[0] - direction[0] * trace_end_offset),
                   static_cast<float>(target[1] - direction[1] * trace_end_offset),
                   static_cast<float>(target[2] - direction[2] * trace_end_offset))};
}
}
namespace diagnostic_detail {
auto vector_string(Vector3f const value) -> std::string {
    return std::format("({}, {}, {})", value.X, value.Y, value.Z);
}
}

/* **************************************** */
// Navigation helpers
/* **************************************** */
auto Sim::get_navigation_tick_period(NavigationRiskTier const tier) const -> std::int16_t {
    auto const index{static_cast<std::int32_t>(tier)};
    assert(index >= 0 && static_cast<std::size_t>(index) < navigation_tick_periods.size());
    return navigation_tick_periods[index];
}
void Sim::reset_navigation_state(std::int32_t const fighter_index,
                                 NavigationRiskTier const initial_tier) {
    auto const data{entity_buffers.current().get_view()};
    assert(fighter_index >= 0 && fighter_index < data.num());
    set_vector(data.view_separation_steering(), fighter_index, HMM_V3(0.f, 0.f, 0.f));
    data.navigation_risk_tiers()[fighter_index] = static_cast<std::uint8_t>(initial_tier);
    data.navigation_lower_risk_scan_counts()[fighter_index] = 0;
    data.avoidance_choice_indices()[fighter_index] = direct_movement_choice;
    data.avoidance_clear_scan_counts()[fighter_index] = 0;
    data.navigation_update_countdowns_periods()[fighter_index] =
        get_navigation_tick_period(initial_tier);
    data.navigation_update_countdowns_remaining_ticks()[fighter_index] = 0;
}

/* **************************************** */
// Configuration
/* **************************************** */
void Sim::set_config(FighterSimConfig const& new_config,
                     FighterLevelData const level_data) noexcept {
    config = new_config;
    collision_radius_ = level_data.collision_radius;
    fire_point_distance_ = level_data.fire_point_distance;

    for (auto& is_participant : participant_mask) {
        is_participant = 0;
    }
    for (auto const team : level_data.participating_teams) {
        auto const team_index{static_cast<std::int32_t>(team)};
        if (team_index >= 0 && static_cast<std::size_t>(team_index) < participant_mask.size()) {
            participant_mask[team_index] = 1;
        }
    }

    assert(std::in_range<std::int32_t>(level_data.participating_teams.size()));
    auto const participant_count{static_cast<std::int32_t>(level_data.participating_teams.size())};
    per_team_limit =
        participant_count == 0 ? 0 : std::max(0, config.max_live_fighters) / participant_count;
}
Sim::Sim(SimClock const& clock,
         EntityLedger& ledger,
         CombatEvents const& combat_events,
         EntityTables& entity_tables,
         AgentAccessor const& agents,
         SpatialQueryManager const& in_spatial_query_manager,
         lasers::Sim& in_laser_simulation) noexcept
    : simulation_clock{clock}
    , ledger_{ledger}
    , combat_events_{combat_events}
    , entity_tables_{entity_tables}
    , agents_{agents}
    , spatial_query_manager{in_spatial_query_manager}
    , laser_simulation{in_laser_simulation} {}

/* **************************************** */
// Sim phases
/* **************************************** */
void Sim::begin_play() {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::begin_play");
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
    awareness_cleaner_ = 0;
    reposition_cleaner_ = 0;
    attack_cleaner_ = 0;
    assert(collision_radius_ > 0.f);
    assert(fire_point_distance_ >= 0.f);

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
        static_cast<float>(get_navigation_tick_period(NavigationRiskTier::Clear) *
                           simulation_clock.get_tick_period())};
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
void Sim::prepare_tick(float const dt) {
    movement_tick_period_ = dt;
    if (!tasks_are_contiguous()) {
        refresh_layout();
    }
    SANDBOX_PROFILE_SCOPE("fighters::Sim::prepare_tick");

    auto const data{entity_buffers.current().get_view()};
    ml::tick_countdowns<std::int8_t>(data.awareness_scan_countdowns(), awareness_cleaner_, 64);
    ml::tick_countdowns<std::int16_t>(
        data.attack_reposition_countdowns(), reposition_cleaner_, 16384);
    ml::tick_periodic_countdowns<std::int16_t>(data.navigation_update_countdowns_remaining_ticks());
    clear_tick_buffers();

    for (auto& capacity : remaining_team_capacity) {
        capacity = per_team_limit;
    }
    for (auto const team : data.teams()) {
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
    ml::tick_countdowns<std::int16_t>(data.attack_cooldowns(), attack_cleaner_, 16384);
}
void Sim::think(float const dt, ml::FrameScratch& scratch) {
    refresh_target_data(scratch);
    SANDBOX_PROFILE_SCOPE("fighters::Sim::think");

    auto const data{entity_buffers.current().get_view()};
    auto const awareness_radius{config.awareness_radius};
    auto const attack_engagement_threshold{config.attack_engagement_threshold};
    auto const attack_engagement_threshold_sq{attack_engagement_threshold *
                                              attack_engagement_threshold};
    auto const n{data.num()};
    std::array<EntityUniqueId, 128> nearby_entities;
    auto const dot_threshold{config.minimum_opportunistic_intercept_deviation_dot_product};
    bool targets_changed{};

    auto const awareness_countdowns{data.awareness_scan_countdowns()};
    auto const locations{data.view_locations()};
    auto const target_ids{data.target_ids()};
    auto const target_distance_sq{data.target_distance_sq()};
    auto const teams{data.teams()};
    auto const aim_directions{data.view_aim_directions()};

    {
        SANDBOX_PROFILE_SCOPE("fighters::Sim::awareness_scan");
        for (std::int32_t i{0}; i < n; ++i) {
            if (!ml::TickCountdownView<std::int8_t>{awareness_countdowns, awareness_restart_ticks_}
                     .try_consume(i)) {
                continue;
            }

            auto const fighter_location{vector_at(locations, i)};
            auto const target_id{target_ids[i]};
            if (agents_.is_alive(target_id) &&
                target_distance_sq[i] <= attack_engagement_threshold_sq) {
                continue;
            }

            auto const n_nearby_entities{spatial_query_manager.collect_non_team_entities_in_range(
                fighter_location, teams[i], awareness_radius, nearby_entities)};
            auto const aim_direction{vector_at(aim_directions, i)};
            EntityUniqueId selected_target{};
            for (std::int32_t nearby_index{}; nearby_index < n_nearby_entities; ++nearby_index) {
                auto const candidate{nearby_entities[nearby_index]};
                auto const state{agents_.read_alive(candidate)};
                if (!state) {
                    continue;
                }
                auto const direction{
                    ml::native_math::safe_normal(state->location - fighter_location, 1.e-8f)};
                if (HMM_DotV3(aim_direction, direction) > dot_threshold) {
                    selected_target = candidate;
                    break;
                }
            }

            if (selected_target.is_valid() && selected_target != target_id) {
                target_ids[i] = selected_target;
                targets_changed = true;
            }
        }
    }
    if (targets_changed) {
        refresh_target_data(scratch);
    }
    plan_movement(dt, scratch);
}
void Sim::plan_movement(float const dt, ml::FrameScratch& scratch) {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::plan_movement");

    auto const d_turn{std::min(1.f, config.turn_speed_unitless * dt)};
    auto const data{entity_buffers.current().get_view()};
    if (data.num() < 1) {
        return;
    }

    copy_vectors(data.view_planned_aim_directions(), data.view_aim_directions().get_const_view());

    auto const move_view{get_task_view(Task::MoveToDestination)};
    auto const attack_view{get_task_view(Task::Attack)};
    auto const do_move{move_view.num() > 0};
    auto const n_attack{attack_view.num()};
    auto const do_attack{n_attack > 0};
    auto const laser_max_distance{config.laser.max_distance};
    auto const& attack_distance_band{config.attack_distance_band};
    auto const desired_attack_distance{laser_max_distance * attack_distance_band.desired_ratio};
    auto const inner_attack_distance{laser_max_distance * attack_distance_band.minimum_ratio};
    auto const outer_attack_distance{laser_max_distance * attack_distance_band.maximum_ratio};

    if (do_attack) {
        auto const locations{attack_view.view_locations()};
        auto const target_locations{attack_view.view_target_locations()};
        auto const target_velocities{attack_view.view_target_velocities()};
        auto const intercept_times{attack_view.intercept_times()};
        auto const desired_aiming_directions{attack_view.view_desired_aiming_directions()};
        auto const desired_move_locations{attack_view.view_desired_move_locations()};
        auto const target_directions{attack_view.view_target_directions()};
        ml::detail::solve_intercept_times_soa_loop::solve_intercept_times(
            intercept_times.data(),
            locations,
            target_locations,
            target_velocities,
            config.laser.projectile_speed);

        auto reposition_countdowns{ml::TickCountdownView<std::int16_t>{
            attack_view.attack_reposition_countdowns(), reposition_restart_ticks_}};
        for (std::int32_t index{}; index < n_attack; ++index) {
            auto const element{static_cast<std::size_t>(index)};
            auto const location{vector_at(locations, index)};
            auto const target_location{vector_at(target_locations, index)};
            auto const intercept_location{target_location + vector_at(target_velocities, index) *
                                                                intercept_times[element]};
            set_vector(desired_aiming_directions,
                       index,
                       ml::native_math::safe_normal(intercept_location - location, 1.e-8f));

            if (!reposition_countdowns.try_consume(element)) {
                continue;
            }
            auto const target_to_move_distance{
                HMM_LenV3(target_location - vector_at(desired_move_locations, index))};
            if (target_to_move_distance >= inner_attack_distance &&
                target_to_move_distance <= outer_attack_distance) {
                continue;
            }
            auto const target_direction{
                ml::native_math::safe_normal(target_location - location, 1.e-8f)};
            set_vector(target_directions, index, target_direction);
            set_vector(desired_move_locations,
                       index,
                       target_location - target_direction * desired_attack_distance);
        }
    }

    auto const locations{data.view_locations()};
    auto const destinations{data.view_desired_move_locations()};
    ml::native_math::direction_and_distance(data.view_movement_directions().xs().data(),
                                            data.view_movement_directions().ys().data(),
                                            data.view_movement_directions().zs().data(),
                                            data.move_distances().data(),
                                            locations.xs().data(),
                                            locations.ys().data(),
                                            locations.zs().data(),
                                            destinations.xs().data(),
                                            destinations.ys().data(),
                                            destinations.zs().data(),
                                            data.num());
    update_navigation_steering(scratch);
    if (do_move) {
        auto const movement_directions{move_view.view_movement_directions()};
        lerp_in_place(move_view.view_planned_aim_directions(), movement_directions, d_turn);
    }
    if (do_attack) {
        auto const choices{attack_view.avoidance_choice_indices()};
        auto const movement_directions{attack_view.view_movement_directions()};
        auto const desired_directions{attack_view.view_desired_aiming_directions()};
        auto const aim_directions{attack_view.view_aim_directions()};
        auto const planned_directions{attack_view.view_planned_aim_directions()};
        for (std::int32_t index{}; index < n_attack; ++index) {
            auto const choice{choices[index]};
            auto const desired_direction{is_avoidance_direction_choice(choice)
                                             ? vector_at(movement_directions, index)
                                             : vector_at(desired_directions, index)};
            auto const current_direction{vector_at(aim_directions, index)};
            set_vector(planned_directions,
                       index,
                       current_direction + (desired_direction - current_direction) * d_turn);
        }
    }

    for (auto const task : {Task::MoveToDestination, Task::Attack}) {
        auto const view{get_task_view(task)};
        auto const count{view.num()};
        auto const move_distances{view.move_distances()};
        auto const speeds{view.speeds()};
        for (std::int32_t index{}; index < count; ++index) {
            move_distances[index] = std::min(move_distances[index], speeds[index] * dt);
        }
    }

    auto const attack_locations{attack_view.view_locations()};
    auto const attack_directions{attack_view.view_movement_directions()};
    auto const attack_move_distances{attack_view.move_distances()};
    auto const attack_targets{attack_view.view_target_locations()};
    auto const attack_target_distance_sq{attack_view.target_distance_sq()};
    auto const attack_target_distances{attack_view.target_distances()};

    for (std::int32_t index{}; index < n_attack; ++index) {
        auto const next_location{vector_at(attack_locations, index) +
                                 vector_at(attack_directions, index) *
                                     attack_move_distances[index]};
        auto const distance_sq{HMM_LenSqrV3(next_location - vector_at(attack_targets, index))};
        attack_target_distance_sq[index] = distance_sq;
        attack_target_distances[index] = std::sqrt(distance_sq);
    }
}
void Sim::apply_movement(ml::FrameScratch& scratch) {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::apply_movement");
    auto const data{entity_buffers.current().get_view()};
    overlap_candidates_.clear();
    auto const count{data.num()};
    auto const aim_directions{data.view_aim_directions()};
    auto const planned_directions{data.view_planned_aim_directions()};
    auto const entity_ids{data.entity_ids()};

    for (std::int32_t index{}; index < count; ++index) {
        auto const direction{vector_at(aim_directions, index)};
        auto const planned_direction{vector_at(planned_directions, index)};
        if (direction.X == planned_direction.X && direction.Y == planned_direction.Y &&
            direction.Z == planned_direction.Z) {
            continue;
        }
        auto const before{direction_to_rotation(direction)};
        auto const after{direction_to_rotation(planned_direction)};
        if (before.pitch != after.pitch || before.yaw != after.yaw || before.roll != after.roll) {
            overlap_candidates_.push_back(entity_ids[index]);
        }
    }
    data.view_velocities().each_column([](auto column) { std::ranges::fill(column, 0.f); });
    copy_vectors(aim_directions, planned_directions.get_const_view());
    move(movement_tick_period_, get_task_view(Task::MoveToDestination), scratch);
    move(movement_tick_period_, get_task_view(Task::Attack), scratch);
    std::ranges::sort(overlap_candidates_);
    auto const duplicates{std::ranges::unique(overlap_candidates_)};
    overlap_candidates_.erase(duplicates.begin(), duplicates.end());

    lasers::FrameSpawnRequests requests{scratch};
    auto const locations{data.view_locations()};
    auto const velocities{data.view_velocities()};
    auto const teams{data.teams()};

    for (auto const index : pending_fire_indices_) {
        auto const direction{vector_at(aim_directions, index)};
        requests.add(vector_at(locations, index) + direction * fire_point_distance_,
                     direction_to_rotation(direction),
                     vector_at(velocities, index),
                     config.laser.damage,
                     config.laser.projectile_speed,
                     config.laser.max_distance,
                     entity_ids[index],
                     {teams[index], EntityType::Fighter});
    }
    laser_simulation.queue_laser_spawns(requests);
    pending_fire_indices_.clear();
}
void Sim::generate_fire_commands(ml::FrameScratch& scratch) {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::generate_fire_commands");
    handle_firing(get_task_view(Task::Attack), scratch);
}
void Sim::resolve_damage_events() {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::resolve_damage_events");

    auto const data{entity_buffers.current().get_view()};
    auto const healths{entity_tables_.health.get_view(data.health_indices(), data.entity_ids())};
    auto const damage_events{combat_events_.events_for(EntityType::Fighter)};
    auto const previous_death_count{entity_death_info.num()};
    batch::resolve_damage_events(damage_events,
                                 agents_.indexes(),
                                 data.entity_ids(),
                                 healths,
                                 local_indices_to_remove,
                                 entity_death_info,
                                 ledger_);

    if (entity_death_info.num() != previous_death_count) {
        ++membership_revision_;
    }

    auto const damage_count{damage_events.num()};
    [[maybe_unused]] auto const entity_ids{data.entity_ids()};
    auto const teams{data.teams()};
    auto const target_ids{data.target_ids()};

    for (std::int32_t event_index{}; event_index < damage_count; ++event_index) {
        auto const event_element{static_cast<std::size_t>(event_index)};
        auto const damaged_id{damage_events.damaged_entities[event_element]};
        assert(damaged_id.is_valid() && damaged_id.entity_type() == EntityType::Fighter);
        auto const fighter_index{agents_.indexes().find(damaged_id)};
        assert(fighter_index >= 0 && fighter_index < data.num());
        assert(entity_ids[fighter_index] == damaged_id);
        if (is_dead(healths.health(fighter_index))) {
            continue;
        }

        auto const instigator{damage_events.instigators[event_element]};
        auto const source{agents_.read_alive(instigator)};
        if (!source) {
            continue;
        }
        if (source->team != teams[fighter_index]) {
            target_ids[fighter_index] = instigator;
        }
    }
}
void Sim::publish_deaths() {
    auto const deaths{entity_death_info.get_const_view()};
    for (std::int32_t i{}; i < deaths.num(); ++i) {
        ledger_.record_death(deaths.victims[i], deaths.killers[i], deaths.reasons[i]);
    }
}
void Sim::remove_components() {
    agents_.indexes().assert_removal_allowed();
    SANDBOX_PROFILE_SCOPE("fighters::Sim::remove_components");

    batch::sort_and_deduplicate_removal_indices(local_indices_to_remove);
    if (local_indices_to_remove.empty()) {
        return;
    }

    auto const columns{entity_buffers.current().get_const_view()};
    entity_tables_.remove_health_rows(
        local_indices_to_remove, columns.health_indices(), columns.entity_ids());
}
void Sim::remove_entities() {
    agents_.indexes().assert_removal_allowed();
    SANDBOX_PROFILE_SCOPE("fighters::Sim::remove_entities");

    remove_dead_entities();
    if (!local_indices_to_remove.empty()) {
        refresh_layout();
    }
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
void Sim::move(float const dt, TaskView fighters, ml::FrameScratch& scratch) {
    assert(dt > 0.f);
    auto const count{fighters.num()};
    auto const directions{fighters.view_movement_directions().get_const_view()};
    FrameVectors3f previous_locations{scratch};
    previous_locations.set_num(count);
    copy_vectors(previous_locations.get_view(), fighters.view_locations().get_const_view());
    auto const move_distances{fighters.move_distances()};
    auto const velocities{fighters.view_velocities()};
    auto const locations{fighters.view_locations()};
    auto const entity_ids{fighters.entity_ids()};
    auto const direction_xs{directions.xs()};
    auto const direction_ys{directions.ys()};
    auto const direction_zs{directions.zs()};
    auto const velocity_xs{velocities.xs()};
    auto const velocity_ys{velocities.ys()};
    auto const velocity_zs{velocities.zs()};

    for (std::int32_t index{}; index < count; ++index) {
        auto const move_distance{move_distances[index]};
        auto const velocity_scale{move_distance / dt};
        velocity_xs[index] = direction_xs[index] * velocity_scale;
        velocity_ys[index] = direction_ys[index] * velocity_scale;
        velocity_zs[index] = direction_zs[index] * velocity_scale;
    }
    ml::native_math::add_scaled_product_in_place(locations.xs().data(),
                                                 locations.ys().data(),
                                                 locations.zs().data(),
                                                 directions.xs().data(),
                                                 directions.ys().data(),
                                                 directions.zs().data(),
                                                 move_distances.data(),
                                                 1.f,
                                                 count);
    auto const before_locations{previous_locations.get_const_view()};
    for (std::int32_t index{}; index < count; ++index) {
        auto const before{before_locations[index]};
        auto const after{vector_at(locations, index)};
        if (before.X != after.X || before.Y != after.Y || before.Z != after.Z) {
            overlap_candidates_.push_back(entity_ids[index]);
        }
    }
}
void Sim::update_navigation_steering(ml::FrameScratch& frame_scratch) {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::update_navigation_steering");

    auto const clearance{collision_radius_ + config.avoidance_clearance_buffer};
    auto const minimum_lookahead_distance{collision_radius_ * 2.f};
    auto const avoidance_lookahead_time{
        std::max(config.avoidance_lookahead_time, minimum_navigation_lookahead_time)};
    auto const active_update_interval{
        static_cast<float>(get_navigation_tick_period(NavigationRiskTier::Active) *
                           simulation_clock.get_tick_period())};
    auto const safe_progress_time{active_update_interval * 1.25f};
    navigation_telemetry = {};
    NavigationScratch scratch{frame_scratch};

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
void Sim::collect_navigation_updates(NavigationScratch& scratch) {
    auto const data{entity_buffers.current().get_view()};
    std::array const active_spans{get_task_span(Task::MoveToDestination),
                                  get_task_span(Task::Attack)};
    ml::PeriodicTickCountdownView<std::int16_t> const countdowns{
        data.navigation_update_countdowns_remaining_ticks(),
        data.navigation_update_countdowns_periods()};
    auto const count{static_cast<std::int32_t>(countdowns.num())};
    scratch.ready_fighter_indices.reserve(count);
    scratch.observed_risk_tiers.set_num(count);

    for (auto const span : active_spans) {
        auto const end{span.end()};
        assert(span.offset >= 0 && span.count >= 0 && end <= count);
        for (auto index{span.offset}; index < end; ++index) {
            auto const element{static_cast<std::size_t>(index)};
            if (!countdowns.try_consume(element)) {
                continue;
            }

            scratch.ready_fighter_indices.add(index);
            scratch.observed_risk_tiers[index] =
                static_cast<std::uint8_t>(NavigationRiskTier::Clear);
        }
    }
}
void Sim::update_separation_observations(NavigationScratch& scratch) {
    auto const data{entity_buffers.current().get_view()};
    auto const healths{
        entity_tables_.health.get_const_view(data.health_indices(), data.entity_ids())};
    // Fixed capacity bounds scoring work and keeps neighbour storage off the heap.
    std::array<EntityUniqueId, max_separation_neighbours> nearby_fighters;
    std::array<SeparationNeighbour, max_separation_neighbours> neighbours;
    auto const separation_radius{config.separation_radius};
    auto const immediate_distance{collision_radius_ * 2.f};
    auto const close_distance{std::max(separation_radius * 0.5f, immediate_distance)};
    auto const immediate_distance_sq{immediate_distance * immediate_distance};
    auto const close_distance_sq{close_distance * close_distance};

    auto const movement_directions{data.view_movement_directions()};
    auto const move_distances{data.move_distances()};
    auto const separation_steering{data.view_separation_steering()};
    auto const choices{data.avoidance_choice_indices()};
    auto const clear_scan_counts{data.avoidance_clear_scan_counts()};
    auto const locations{data.view_locations()};
    auto const entity_ids{data.entity_ids()};
    auto const risk_tiers{data.navigation_risk_tiers()};
    auto const integral_biases{data.integral_biases()};
    auto const float_biases{data.float_biases()};

    for (auto const fighter_index : scratch.ready_fighter_indices) {
        auto const goal_direction{vector_at(movement_directions, fighter_index)};
        auto const move_distance{move_distances[fighter_index]};
        if (ml::native_math::is_nearly_zero(
                goal_direction.X, goal_direction.Y, goal_direction.Z, 1.e-4f) ||
            move_distance <= 0.f) {
            set_vector(separation_steering, fighter_index, Vector3f{});
            choices[fighter_index] = direct_movement_choice;
            clear_scan_counts[fighter_index] = 0;
            continue;
        }

        auto const fighter_location{vector_at(locations, fighter_index)};
        auto const fighter_id{entity_ids[fighter_index]};
        auto const n_nearby{spatial_query_manager.collect_entities_of_type_in_range(
            fighter_location, EntityType::Fighter, separation_radius, fighter_id, nearby_fighters)};
        ++navigation_telemetry.separation_query_count;
        navigation_telemetry.separation_candidate_count += n_nearby;

        std::int32_t neighbour_count{};
        for (std::int32_t index{}; index < n_nearby; ++index) {
            auto const local_index{agents_.indexes().find(nearby_fighters[index])};
            if (local_index >= 0 && is_alive(healths.health(local_index))) {
                neighbours[neighbour_count++] = {entity_ids[local_index],
                                                 vector_at(locations, local_index)};
            }
        }

        auto const previous_memory{vector_at(separation_steering, fighter_index)};
        auto const current_tier{static_cast<NavigationRiskTier>(risk_tiers[fighter_index])};
        auto const elapsed_since_scan{static_cast<float>(get_navigation_tick_period(current_tier)) *
                                      simulation_clock.get_tick_period()};
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
            {neighbours.data(), static_cast<std::size_t>(neighbour_count)},
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
    auto const data{entity_buffers.current().get_view()};
    std::array const active_spans{get_task_span(Task::MoveToDestination),
                                  get_task_span(Task::Attack)};
    auto const movement_directions{data.view_movement_directions()};
    auto const separation_steering{data.view_separation_steering()};

    for (auto const span : active_spans) {
        auto const end{span.end()};
        assert(span.offset >= 0 && span.count >= 0 && end <= data.num());
        for (auto index{span.offset}; index < end; ++index) {
            auto const direction{
                make_separation_steering_direction(vector_at(movement_directions, index),
                                                   vector_at(separation_steering, index),
                                                   config.separation_strength)};
            if (direction) {
                set_vector(movement_directions, index, *direction);
            }
        }
    }
}
void Sim::scan_preferred_navigation(NavigationScratch& scratch,
                                    float const clearance,
                                    float const avoidance_lookahead_time,
                                    float const minimum_lookahead_distance) {
    auto const data{entity_buffers.current().get_view()};
    auto const count{scratch.ready_fighter_indices.num()};
    scratch.line_of_sight_starts.reserve(count);
    scratch.line_of_sight_ends.reserve(count);
    scratch.trace_fighter_indices.reserve(count);
    scratch.blocked_fighter_indices.reserve(count);

    auto const movement_directions{data.view_movement_directions()};
    auto const move_distances{data.move_distances()};
    auto const speeds{data.speeds()};
    auto const locations{data.view_locations()};

    for (auto const index : scratch.ready_fighter_indices) {
        auto const element{static_cast<std::size_t>(index)};
        auto const direction{vector_at(movement_directions, index)};
        auto const move_distance{move_distances[element]};
        if (ml::native_math::is_nearly_zero(direction.X, direction.Y, direction.Z, 1.e-4f) ||
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
        execute_navigation_sweeps(scratch, clearance);

        for (std::int32_t index{}; index < n_direct_traces; ++index) {
            auto const fighter_index{scratch.trace_fighter_indices[index]};
            auto const element{static_cast<std::size_t>(fighter_index)};
            if (scratch.line_of_sight_results[index] == 0 || scratch.trace_hits.hits[index] != 0) {
                scratch.blocked_fighter_indices.add(fighter_index);
                clear_scan_counts[element] = 0;
                continue;
            }
            if (choices[element] == direct_movement_choice) {
                clear_scan_counts[element] = 0;
                continue;
            }

            auto& clear_count{clear_scan_counts[element]};
            ++clear_count;
            if (clear_count >= clear_scans_to_end_avoidance) {
                choices[element] = direct_movement_choice;
                clear_count = 0;
            }
        }
    }
}
void Sim::scan_alternative_navigation(NavigationScratch& scratch,
                                      float const clearance,
                                      float const avoidance_lookahead_time,
                                      float const minimum_lookahead_distance) {
    auto const data{entity_buffers.current().get_const_view()};
    scratch.trace_fighter_indices.clear();
    scratch.trace_choice_indices.clear();
    scratch.line_of_sight_starts.clear();
    scratch.line_of_sight_ends.clear();
    scratch.line_of_sight_results.clear();
    scratch.trace_hits.clear();

    auto const count{scratch.blocked_fighter_indices.num() * n_avoidance_choices};
    scratch.line_of_sight_starts.reserve(count);
    scratch.line_of_sight_ends.reserve(count);
    scratch.trace_choice_indices.reserve(count);
    auto const speeds{data.speeds()};
    auto const move_distances{data.move_distances()};
    auto const locations{data.view_locations()};
    auto const movement_directions{data.view_movement_directions()};
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
        make_avoidance_choice_order(integral_biases[element], choices[element], order);

        for (auto const choice : order) {
            auto const direction{directions[static_cast<std::size_t>(choice)]};
            scratch.line_of_sight_starts.add(start);
            scratch.line_of_sight_ends.add(start + direction * lookahead_distance);
            scratch.trace_choice_indices.add(choice);
        }
    }
    assert(scratch.line_of_sight_ends.num() == count);
    execute_navigation_sweeps(scratch, clearance);
}
void Sim::execute_navigation_sweeps(NavigationScratch& scratch, float const clearance) {
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
                                              collision::TraceEntityFilter::ExcludeFighters);
    navigation_telemetry.hard_trace_count += trace_count;
}
void Sim::select_navigation_alternatives(NavigationScratch& scratch,
                                         float const safe_progress_time) {
    auto const data{entity_buffers.current().get_view()};
    if (!diagnostics_enabled_) {
        diagnostic_stop_reports = 0;
    }
    auto const n_blocked_fighters{scratch.blocked_fighter_indices.num()};
    auto const locations{data.view_locations()};
    auto const speeds{data.speeds()};
    auto const choices{data.avoidance_choice_indices()};

    for (std::int32_t blocked_index{}; blocked_index < n_blocked_fighters; ++blocked_index) {
        auto const fighter_index{scratch.blocked_fighter_indices[blocked_index]};
        auto const fighter_location{vector_at(locations, fighter_index)};
        // Partial progress must leave room until the next active scan, including its margin.
        auto const safe_progress_distance{speeds[fighter_index] * safe_progress_time};

        auto const candidate_begin{blocked_index * n_avoidance_choices};
        auto const candidate_end{candidate_begin + n_avoidance_choices};
        auto const candidate_count{static_cast<std::size_t>(n_avoidance_choices)};
        auto const chosen_choice{fighters::choose_navigation_alternative(
            fighter_location,
            safe_progress_distance,
            {scratch.trace_choice_indices.data() + candidate_begin, candidate_count},
            {scratch.line_of_sight_results.data() + candidate_begin, candidate_count},
            scratch.trace_hits.hits.view().subspan(candidate_begin, candidate_count),
            scratch.trace_hits.locations.get_const_view().slice(candidate_begin,
                                                                n_avoidance_choices),
            stop_movement_choice)};

        choices[fighter_index] = chosen_choice;
        if (chosen_choice == stop_movement_choice &&
            diagnostics::take_report(diagnostics_enabled_, diagnostic_stop_reports, 8)) {
            ml::log_error(
                std::format("[FighterStop] fighterId={} position={} destination={} "
                            "preferred={} separation={} clearance={:.2f} safeTravel={:.2f} risk={}",
                            data.entity_ids()[fighter_index].raw_value(),
                            diagnostic_detail::vector_string(fighter_location),
                            diagnostic_detail::vector_string(
                                vector_at(data.view_desired_move_locations(), fighter_index)),
                            diagnostic_detail::vector_string(
                                vector_at(data.view_movement_directions(), fighter_index)),
                            diagnostic_detail::vector_string(
                                vector_at(data.view_separation_steering(), fighter_index)),
                            collision_radius_ + config.avoidance_clearance_buffer,
                            safe_progress_distance,
                            data.navigation_risk_tiers()[fighter_index]));
            for (std::int32_t trace_index{candidate_begin}; trace_index < candidate_end;
                 ++trace_index) {
                ml::log_error(std::format(
                    "[FighterStop] choice={} end={} inWorld={} hit={} "
                    "blockerId={} staticIndex={} hitDistance={:.2f}",
                    scratch.trace_choice_indices[trace_index],
                    diagnostic_detail::vector_string(
                        scratch.line_of_sight_ends.get_const_view()[trace_index]),
                    scratch.line_of_sight_results[trace_index],
                    scratch.trace_hits.hits[trace_index],
                    scratch.trace_hits.entities[trace_index].raw_value(),
                    scratch.trace_hits.static_geometry_indices[trace_index],
                    scratch.trace_hits.hits[trace_index]
                        ? HMM_LenV3(fighter_location -
                                    scratch.trace_hits.locations.get_const_view()[trace_index])
                        : -1.f));
            }
        }
    }
}
void Sim::apply_navigation_choices(NavigationScratch const& scratch) {
    auto const data{entity_buffers.current().get_view()};
    std::array const active_spans{get_task_span(Task::MoveToDestination),
                                  get_task_span(Task::Attack)};
    auto const choices{data.avoidance_choice_indices()};
    auto const risk_tiers{data.navigation_risk_tiers()};
    auto const lower_risk_scan_counts{data.navigation_lower_risk_scan_counts()};
    auto const countdown_periods{data.navigation_update_countdowns_periods()};
    auto const remaining_ticks{data.navigation_update_countdowns_remaining_ticks()};

    for (auto const index : scratch.ready_fighter_indices) {
        auto const element{static_cast<std::size_t>(index)};
        auto observed{static_cast<NavigationRiskTier>(scratch.observed_risk_tiers[index])};
        if (choices[element] != direct_movement_choice) {
            observed = std::max(observed, NavigationRiskTier::Active);
        }
        auto const update{
            update_navigation_risk(static_cast<NavigationRiskTier>(risk_tiers[element]),
                                   observed,
                                   lower_risk_scan_counts[element],
                                   lower_risk_scans_to_demote)};
        risk_tiers[element] = static_cast<std::uint8_t>(update.tier);
        lower_risk_scan_counts[element] = update.lower_risk_scan_count;
        auto const period{get_navigation_tick_period(update.tier)};
        countdown_periods[element] = period;
        remaining_ticks[element] = period;
    }

    auto const separation_steering{data.view_separation_steering()};
    auto const movement_directions{data.view_movement_directions()};
    auto const float_biases{data.float_biases()};

    for (auto const span : active_spans) {
        auto const end{span.end()};
        assert(span.offset >= 0 && span.count >= 0 && end <= data.num());
        for (auto index{span.offset}; index < end; ++index) {
            auto const element{static_cast<std::size_t>(index)};
            auto const steering{vector_at(separation_steering, index)};
            if (!ml::native_math::is_nearly_zero(steering.X, steering.Y, steering.Z, 1.e-4f)) {
                ++navigation_telemetry.separating_fighter_count;
                ++navigation_telemetry.steering_memory_fighter_count;
            }
            auto const choice{choices[element]};
            if (is_avoidance_direction_choice(choice)) {
                auto const frame{make_avoidance_frame(vector_at(movement_directions, index),
                                                      float_biases[element])};
                set_vector(movement_directions, index, make_avoidance_direction(frame, choice));
                ++navigation_telemetry.avoiding_fighter_count;
            } else if (choice == stop_movement_choice) {
                set_vector(movement_directions, index, HMM_V3(0.f, 0.f, 0.f));
                ++navigation_telemetry.avoiding_fighter_count;
            }

            switch (static_cast<NavigationRiskTier>(risk_tiers[element])) {
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
auto Sim::get_num_instances() const noexcept -> std::int32_t {
    return entity_buffers.current().num();
}
void Sim::set_parent_id(EntityUniqueId const fighter, EntityUniqueId const parent) {
    assert(fighter.is_valid() && fighter.entity_type() == EntityType::Fighter);
    auto const index{agents_.indexes().find(fighter)};
    assert(index >= 0);
    auto const data{entity_buffers.current().get_view()};

    if (data.parent_ids()[index] == parent) {
        return;
    }

    data.parent_ids()[index] = parent;
    if (is_alive(entity_tables_.health.get_health(data.health_indices()[index], fighter))) {
        ++membership_revision_;
    }
}
auto Sim::get_view(std::int32_t const offset, std::int32_t const width) -> EntityStorage::View {
    return entity_buffers.current().get_view(offset, width);
}
auto Sim::get_const_view(std::int32_t const offset, std::int32_t const width) const
    -> EntityStorage::ConstView {
    return entity_buffers.current().get_const_view(offset, width);
}
auto Sim::has_id(EntityUniqueId const fighter) const -> bool {
    return find_index(fighter) != -1;
}
auto Sim::get_target_ids() const noexcept -> std::span<EntityUniqueId const> {
    return entity_buffers.current().get_const_view().target_ids();
}
auto Sim::get_target_id(EntityUniqueId const fighter) const noexcept -> EntityUniqueId {
    return entity_buffers.current().get_const_view().target_ids()[find_index(fighter)];
}
auto Sim::get_target_location(EntityUniqueId const fighter) const -> Vector3f {
    return vector_at(entity_buffers.current().get_const_view().view_target_locations(),
                     find_index(fighter));
}
auto Sim::get_tasks() const -> std::span<Task const> {
    return entity_buffers.current().get_const_view().tasks();
}
auto Sim::get_teams() const -> std::span<Team const> {
    return entity_buffers.current().get_const_view().teams();
}
auto Sim::get_task_spans() const -> TaskSpans {
    check_fighter_tasks();
    return task_spans;
}
auto Sim::get_task_counts() const -> TaskCounts {
    auto const data{entity_buffers.current().get_const_view()};
    TaskCounts counts{};
    for (auto const task : data.tasks()) {
        auto const group{static_cast<std::size_t>(task)};
        assert(group < n_task_types);
        ++counts[group];
    }
    return counts;
}
auto Sim::get_task_view(Task const task) noexcept -> TaskView {
    auto const span{get_task_span(task)};
    return entity_buffers.current().get_view(span.offset, span.count);
}
auto Sim::get_const_task_view(Task const task) const noexcept -> ConstTaskView {
    auto const span{get_task_span(task)};
    return entity_buffers.current().get_const_view(span.offset, span.count);
}
auto Sim::find_index(EntityUniqueId const fighter) const noexcept -> std::int32_t {
    return fighter.is_valid() && fighter.entity_type() == EntityType::Fighter
             ? agents_.indexes().find(fighter)
             : -1;
}
auto Sim::get_task_span(Task const task) const -> IndexSpan {
    return task_spans[std::to_underlying(task)];
}

/* **************************************** */
// Targets
/* **************************************** */
void Sim::set_target_id_unchecked(std::int32_t const fighter_index,
                                  EntityUniqueId const new_target) noexcept {
    entity_buffers.current().get_view().target_ids()[fighter_index] = new_target;
}
void Sim::set_target_id(EntityUniqueId const fighter, EntityUniqueId const new_target) noexcept {
    set_target_id_unchecked(find_index(fighter), new_target);
}
void Sim::refresh_target_data(ml::FrameScratch& scratch) {
    auto const data{entity_buffers.current().get_view()};
    auto const count{data.num()};
    ml::FrameArray<std::int32_t> order{&scratch};
    ml::FrameArray<std::uint8_t> alive{&scratch};
    order.set_num(count);
    alive.set_num(count);
    auto const target_ids{data.target_ids()};
    auto const target_radii{data.target_radii()};

    for (std::int32_t index{}; index < count; ++index) {
        auto const target_id{target_ids[index]};
        target_radii[index] =
            target_id.is_valid()
                ? spatial_query_manager.get_entity_type_radius(target_id.entity_type())
                : 0.f;
    }
    agents_.gather_targets(target_ids,
                           order,
                           {{data.view_target_locations().xs(),
                             data.view_target_locations().ys(),
                             data.view_target_locations().zs()},
                            {data.view_target_velocities().xs(),
                             data.view_target_velocities().ys(),
                             data.view_target_velocities().zs()},
                            {},
                            alive});
    for (std::int32_t index{}; index < count; ++index) {
        if (!alive[index]) {
            target_ids[index] = {};
            target_radii[index] = 0.f;
        }
    }
    distance_and_squared(data.target_distances(),
                         data.target_distance_sq(),
                         data.view_locations().get_const_view(),
                         data.view_target_locations().get_const_view());
}

/* **************************************** */
// Tasks
/* **************************************** */
void Sim::set_task_unchecked(std::int32_t const index, Task const task) noexcept {
    auto const data{entity_buffers.current().get_view()};
    if (data.tasks()[index] == task) {
        return;
    }

    data.tasks()[index] = task;
    reset_navigation_state(
        index, task == Task::Standby ? NavigationRiskTier::Clear : NavigationRiskTier::Nearby);
}
void Sim::set_task(EntityUniqueId const fighter, Task const task) noexcept {
    order_queue.add(fighter, FighterOrder{1, 0}, task, {});
}
bool Sim::tasks_are_contiguous() const noexcept {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::tasks_are_contiguous");

    auto const data{entity_buffers.current().get_const_view()};
    auto current_task{Task::Standby};
    for (auto const task : data.tasks()) {
        if (task < current_task || task >= Task::COUNT) {
            return false;
        }
        current_task = task;
    }

    std::int32_t offset{};
    auto const counts{get_task_counts()};
    for (std::size_t group{}; group < n_task_types; ++group) {
        if (task_spans[group] != IndexSpan{offset, counts[group]}) {
            return false;
        }
        offset += counts[group];
    }
    return true;
}
void Sim::refresh_layout() {
    agents_.indexes().assert_structural_mutation_allowed();
    SANDBOX_PROFILE_SCOPE("fighters::Sim::refresh_layout");

    auto const task_counts{get_task_counts()};
    auto const n_fighters{get_num_instances()};
    TaskCounts write_indices{};
    std::int32_t offset{};
    for (std::size_t group{}; group < n_task_types; ++group) {
        write_indices[group] = offset;
        task_spans[group] = {offset, task_counts[group]};
        offset += task_counts[group];
    }
    assert(task_spans.back().end() == n_fighters);

    entity_buffers.cycle();
    auto const& old_data{entity_buffers.previous()};
    auto& new_data{entity_buffers.current()};
    new_data.reset();
    new_data.reserve(n_fighters);
    auto const old_tasks{old_data.get_const_view().tasks()};
    bool reordered{};
    for (std::size_t group{}; group < n_task_types; ++group) {
        for (std::int32_t index{}; index < n_fighters; ++index) {
            if (static_cast<std::size_t>(old_tasks[index]) == group) {
                reordered |= new_data.num() != index;
                new_data.append_from(old_data.slice(index, 1));
                ++write_indices[group];
            }
        }
    }
    if (reordered) {
        ++layout_revision_;
    }
    assert(old_data.num() == new_data.num());
    check_fighter_tasks();
}

/* **************************************** */
// Spawning
/* **************************************** */
auto Sim::accept_spawn_count(std::span<Team const> const teams) -> std::int32_t {
    if (teams.empty()) {
        return 0;
    }
    auto const team{teams.front()};
    auto const team_index{static_cast<std::size_t>(team)};
    if (team_index >= participant_mask.size() || participant_mask[team_index] == 0) {
        ml::log_error(
            std::format("Rejected fighter spawn request for invalid or non-participating team {}",
                        static_cast<std::int32_t>(teams[0])));
        return 0;
    }

    for (auto const queued_team : teams) {
        if (queued_team != team) {
            ml::log_error("Rejected fighter spawn wave containing multiple teams");
            return 0;
        }
    }

    auto& capacity{remaining_team_capacity[team_index]};
    assert(capacity >= 0);
    auto const accepted_count{std::min(static_cast<std::int32_t>(teams.size()), capacity)};
    capacity -= accepted_count;
    return accepted_count;
}
auto Sim::queue_spawns(SingleAllocationFighterSpawnQueue::ConstView const new_spawns)
    -> std::int32_t {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::queue_spawns");
    new_spawns.validate();
    auto const count{accept_spawn_count(new_spawns.teams())};
    spawn_queue.append_from(new_spawns, 0, count);
    return count;
}
auto Sim::queue_spawns(FrameSpawnQueue const& new_spawns) -> std::int32_t {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::queue_spawns");
    new_spawns.validate();
    auto const count{accept_spawn_count(new_spawns.teams())};
    spawn_queue.append_from(new_spawns, 0, count);
    return count;
}
void Sim::reassign_pending_spawns(EntityUniqueId const parent, EntityUniqueId const replacement) {
    assert(parent.is_valid() && parent.entity_type() == EntityType::CapitalShip);
    assert(!replacement.is_valid() || replacement.entity_type() == EntityType::CapitalShip);

    auto const pending{spawn_queue.get_view()};
    auto const pending_count{pending.num()};
    auto const parents{pending.parents()};

    for (std::int32_t index{}; index < pending_count; ++index) {
        if (parents[index] == parent) {
            parents[index] = replacement;
        }
    }
}
void Sim::commit_spawns() {
    agents_.indexes().assert_preparation_mutation_allowed();
    SANDBOX_PROFILE_SCOPE("fighters::Sim::commit_spawns");

    if (!diagnostics_enabled_) {
        diagnostic_spawn_reports = 0;
    }
    auto const n_cur{get_num_instances()};
    auto const n_new{spawn_queue.num()};

    spawn_queue.get_const_view().validate();
    if (n_new < 1) {
        return;
    }

    entity_buffers.current().add_defaulted(n_new);
    auto const data{entity_buffers.current().get_view()};
    auto const new_data{data.get_view(n_cur, n_new)};
    auto const spawns{spawn_queue.get_const_view()};
    auto const navigation_period{get_navigation_tick_period(NavigationRiskTier::Nearby)};
    copy_vectors(new_data.view_locations(), spawns.view_locations());
    copy_vectors(new_data.view_desired_move_locations(), spawns.view_locations());
    std::ranges::fill(new_data.tasks(), FighterTask::Attack);
    std::ranges::fill(new_data.speeds(), config.speed);
    std::ranges::copy(spawns.teams(), new_data.teams().begin());
    std::ranges::copy(spawns.parents(), new_data.parent_ids().begin());
    std::ranges::copy(spawns.targets(), new_data.target_ids().begin());
    std::ranges::fill(new_data.navigation_risk_tiers(),
                      static_cast<std::uint8_t>(NavigationRiskTier::Nearby));
    std::ranges::fill(new_data.avoidance_choice_indices(), direct_movement_choice);
    std::ranges::fill(new_data.navigation_update_countdowns_periods(), navigation_period);

    auto const aim_directions{new_data.view_aim_directions()};
    auto const pitches{spawns.view_rotations().pitches()};
    auto const yaws{spawns.view_rotations().yaws()};
    auto const rolls{spawns.view_rotations().rolls()};
    for (std::int32_t index{}; index < n_new; ++index) {
        set_vector(aim_directions,
                   index,
                   forward_direction(Rotator3f{pitches[index], yaws[index], rolls[index]}));
    }

    auto const entity_ids{new_data.entity_ids()};
    auto const teams{new_data.teams()};
    for (std::int32_t i{0}; i < n_new; ++i) {
        entity_ids[i] =
            ledger_.record_spawn(EntityType::Fighter, teams[i], is_alive(config.health));
    }
    entity_tables_.health.add(
        std::span<EntityUniqueId const>{data.entity_ids()}.subspan(n_cur, n_new),
        config.health,
        std::span<HealthIndex>{data.health_indices()}.subspan(n_cur, n_new));
    if (is_alive(config.health)) {
        ++membership_revision_;
    }
    if (diagnostics_enabled_) {
        for (std::int32_t i{}; i < n_new; ++i) {
            if (!diagnostics::take_report(diagnostics_enabled_, diagnostic_spawn_reports, 64)) {
                break;
            }
            auto const index{n_cur + i};
            ml::log_error(std::format(
                "[FighterSpawn] Committed fighterId={} parentId={} "
                "targetId={} world={}",
                data.entity_ids()[index].raw_value(),
                data.parent_ids()[index].raw_value(),
                data.target_ids()[index].raw_value(),
                diagnostic_detail::vector_string(vector_at(data.view_locations(), index))));
        }
    }
    make_deterministic_biases(
        std::span<EntityUniqueId const>{data.entity_ids()}.subspan(n_cur, n_new),
        std::span<std::uint32_t>{data.integral_biases()}.subspan(n_cur, n_new),
        std::span<float>{data.float_biases()}.subspan(n_cur, n_new));
}

/* **************************************** */
// Destruction
/* **************************************** */
void Sim::remove_dead_entities() {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::remove_dead_entities");
    auto& data{entity_buffers.current()};
    auto const columns{data.get_const_view()};
    auto const entity_ids{columns.entity_ids()};

    for (auto const index : local_indices_to_remove) {
        agents_.indexes().retire(entity_ids[index]);
    }
    data.remove_at_swap(local_indices_to_remove);
    if (!local_indices_to_remove.empty()) {
        ++layout_revision_;
    }
}

/* **************************************** */
// Combat
/* **************************************** */
void Sim::handle_firing(TaskView data, ml::FrameScratch& scratch) {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::handle_firing");

    auto const locations{data.view_locations()};
    auto const movement_directions{data.view_movement_directions()};
    auto const move_distances{data.move_distances()};

    auto predicted_location =
        [locations, movement_directions, move_distances](std::int32_t const index) {
            return vector_at(locations, index) +
                   vector_at(movement_directions, index) * move_distances[index];
        };

    auto const n_ships{data.num()};
    auto const aim_threshold{config.fire_dot_product_threshold};
    auto const laser_max_distance{config.laser.max_distance};
    auto const laser_max_distance_sq{laser_max_distance * laser_max_distance};
    auto const desired_attack_distance{laser_max_distance *
                                       config.attack_distance_band.desired_ratio};
    auto const arrival_distance{config.arrival_distance};
    auto const attack_position_arrival_distance_sq{arrival_distance * arrival_distance};
    ml::FrameArray<float> aiming_dot_products{&scratch};
    ml::FrameArray<std::int32_t> can_fire{&scratch};
    FrameVectors3f line_of_sight_starts{scratch};
    FrameVectors3f line_of_sight_ends{scratch};
    ml::FrameArray<LineQueryResult> line_of_sight_results{&scratch};
    ml::FrameArray<EntityUniqueId> firing_ignored_entities{&scratch};
    ml::FrameArray<std::int32_t> firing_position_fighter_indices{&scratch};
    FrameVectors3f firing_position_candidates{scratch};

    firing_position_fighter_indices.reserve(n_ships);
    firing_position_candidates.reserve(n_ships);

    auto const los_check_buffer{config.los_check_buffer};
    ml::TickCountdownView<std::int16_t> const cooldowns{
        std::span<std::int16_t>{data.attack_cooldowns()}, attack_retry_cooldown_tick_value};
    aiming_dot_products.set_num(n_ships);
    ml::native_math::dot_product_vector(aiming_dot_products.data(),
                                        data.view_planned_aim_directions().xs().data(),
                                        data.view_planned_aim_directions().ys().data(),
                                        data.view_planned_aim_directions().zs().data(),
                                        data.view_desired_aiming_directions().xs().data(),
                                        data.view_desired_aiming_directions().ys().data(),
                                        data.view_desired_aiming_directions().zs().data(),
                                        n_ships);

    auto const target_distance_sq{data.target_distance_sq()};
    auto const target_ids{data.target_ids()};

    can_fire.reserve(n_ships);
    for (std::int32_t index{}; index < n_ships; ++index) {
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

    auto const planned_directions{data.view_planned_aim_directions()};
    auto const target_radii{data.target_radii()};
    auto const entity_ids{data.entity_ids()};
    auto const target_locations{data.view_target_locations()};

    auto const firing_count{can_fire.num()};
    line_of_sight_starts.set_num(firing_count);
    line_of_sight_ends.set_num(firing_count);
    firing_ignored_entities.set_num(firing_count);
    for (std::int32_t index{}; index < firing_count; ++index) {
        auto const fighter_index{can_fire[index]};
        auto const element{static_cast<std::size_t>(fighter_index)};
        auto const direction{vector_at(planned_directions, fighter_index)};
        auto const end_offset{los_check_buffer + target_radii[element]};
        firing_ignored_entities[index] = entity_ids[element];
        line_of_sight_starts.set(
            index, predicted_location(fighter_index) + direction * fire_point_distance_);
        line_of_sight_ends.set(index,
                               vector_at(target_locations, fighter_index) - direction * end_offset);
    }

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

    auto const desired_move_locations{data.view_desired_move_locations()};

    for (auto index{can_fire.num() - 1}; index >= 0; --index) {
        auto const fighter_index{can_fire[index]};
        if (line_of_sight_results[index] != 0) {
            continue;
        }

        can_fire.remove_at_swap(index);
        cooldowns.restart_counter(static_cast<std::size_t>(fighter_index));
        auto const offset{predicted_location(fighter_index) -
                          vector_at(desired_move_locations, fighter_index)};
        if (HMM_LenSqrV3(offset) <= attack_position_arrival_distance_sq) {
            firing_position_fighter_indices.add(fighter_index);
        }
    }

    auto const integral_biases{data.integral_biases()};
    auto const float_biases{data.float_biases()};

    auto const n_fire_point_candidates{
        static_cast<std::uint32_t>(firing_detail::angle_offsets.size())};
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
            firing_ignored_entities[i] = entity_ids[ship_index];
            auto const candidate{firing_detail::make_fire_point_candidate(
                vector_at(target_locations, ship_index),
                vector_at(desired_move_locations, ship_index),
                fire_point_distance_,
                los_check_buffer + target_radii[ship_index],
                desired_attack_distance,
                integral_biases[ship_index],
                float_biases[ship_index],
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

        for (auto index{n_fighters - 1}; index >= 0; --index) {
            if (line_of_sight_results[index] == 0) {
                continue;
            }

            auto const fighter_index{firing_position_fighter_indices[index]};
            set_vector(desired_move_locations,
                       fighter_index,
                       firing_position_candidates.get_const_view()[index]);
            firing_position_fighter_indices.remove_at_swap(index);
        }
    }

    auto const attack_cooldowns{data.attack_cooldowns()};

    auto const n_can_fire{can_fire.num()};
    auto const attack_offset{get_task_span(Task::Attack).offset};
    for (std::int32_t i{0}; i < n_can_fire; ++i) {
        auto const ship_index{can_fire[i]};
        pending_fire_indices_.push_back(attack_offset + ship_index);
        attack_cooldowns[ship_index] = attack_restart_ticks_;
    }
}

/* **************************************** */
// Orders
/* **************************************** */
void Sim::queue_orders(FighterOrderQueue const& queue) {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::queue_orders");
    order_queue.append_from(queue.get_const_view());
}
void Sim::commit_orders() {
    assert(simulation_clock.phase == SimulationPhase::Preparation);
    SANDBOX_PROFILE_SCOPE("fighters::Sim::commit_orders");

    auto const data{entity_buffers.current().get_view()};
    auto const healths{
        entity_tables_.health.get_const_view(data.health_indices(), data.entity_ids())};
    auto const n_orders{order_queue.num()};
    if (n_orders < 1) {
        return;
    }

    auto const orders{order_queue.get_const_view()};
    auto const tasks{data.tasks()};
    auto const desired_move_locations{data.view_desired_move_locations()};
    auto const locations{data.view_locations()};
    auto const attack_reposition_countdowns{data.attack_reposition_countdowns()};
    auto const target_ids{data.target_ids()};

    for (std::int32_t index{}; index < n_orders; ++index) {
        auto const order_index{static_cast<std::size_t>(index)};
        auto const id{orders.entity_ids[order_index]};
        if (!id.is_valid() || id.entity_type() != EntityType::Fighter) {
            continue;
        }
        auto const fighter_index{agents_.indexes().find(id)};
        if (fighter_index < 0 || is_dead(healths.health(fighter_index))) {
            continue;
        }

        auto const element{static_cast<std::size_t>(fighter_index)};
        auto const order{orders.orders[order_index]};
        if (order.task()) {
            auto const old_task{tasks[element]};
            auto const new_task{orders.tasks[order_index]};
            tasks[element] = new_task;
            reset_navigation_state(fighter_index,
                                   new_task == FighterTask::Standby ? NavigationRiskTier::Clear
                                                                    : NavigationRiskTier::Nearby);
            if (old_task != FighterTask::Attack && new_task == FighterTask::Attack) {
                set_vector(
                    desired_move_locations, fighter_index, vector_at(locations, fighter_index));
                attack_reposition_countdowns[element] = 0;
            }
        }
        if (order.target()) {
            target_ids[element] = orders.targets[order_index];
        }
    }
    order_queue.reset();
}

/* **************************************** */
// Misc
/* **************************************** */
void Sim::clear_tick_buffers() {
    local_indices_to_remove.clear();
    entity_death_info.reset();
    spawn_queue.reset();
}

/* **************************************** */
// Checks
/* **************************************** */
#ifndef NDEBUG
void Sim::check_fighter_tasks() const {
    SANDBOX_PROFILE_SCOPE("fighters::Sim::check_fighter_tasks");

    auto current_task_group{Task::Standby};
    TaskSpans checked_task_spans{};
    auto const data{entity_buffers.current().get_const_view()};
    auto const n_tasks{data.num()};
    auto const tasks{data.tasks()};

    for (std::int32_t i{}; i < n_tasks; ++i) {
        auto const task{tasks[i]};
        auto const task_value{std::to_underlying(task)};
        if (task == current_task_group) {
            ++checked_task_spans[task_value].count;
        } else if (task > current_task_group) {
            current_task_group = task;
            checked_task_spans[task_value].offset = i;
            checked_task_spans[task_value].count = 1;
        } else {
            ml::fatal_error(std::format("Found task {} when current group was {}",
                                        to_string_view(task),
                                        to_string_view(current_task_group)));
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
                                   to_string_view(static_cast<Task>(i)),
                                   task_spans[i].offset,
                                   task_spans[i].count,
                                   checked_task_spans[i].offset,
                                   checked_task_spans[i].count);
        }
        ml::fatal_error(message);
    }
}
#endif
} // namespace fighters
