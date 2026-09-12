#include "SpaceGameSimulation/ships/fighters/TestCapitalShipFightersSimulation.h"

#include <SpaceGameSimulation/combat/lasers/TestLasersFrameScratch.h>
#include <SpaceGameSimulation/entities/BatchSimulation.h>
#include <SpaceGameSimulation/simulation/FighterDiagnostics.h>
#include <SpaceGameSimulation/simulation/FrameTraceHits.h>
#include <SpaceGameSimulation/simulation/LevelSimulationConfig.h>
#include <SpaceGameSimulation/simulation/SpatialQueryManager.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/frame_array.h>
#include <SandboxCore/frame_vectors.h>
#include <SandboxCore/projectile_intercept.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxNative/deterministic_bias.h>

#include <Misc/Optional.h>
#include <ProfilingDebugging/CountersTrace.h>

TRACE_DECLARE_INT_COUNTER(SandboxTestFighterCount, TEXT("Sandbox/TestFighterCount"));
TRACE_DECLARE_INT_COUNTER(SandboxFightersAvoiding, TEXT("Sandbox/FightersAvoiding"));
TRACE_DECLARE_INT_COUNTER(SandboxFighterNavigationTraces, TEXT("Sandbox/FighterNavigationTraces"));
TRACE_DECLARE_INT_COUNTER(SandboxFightersSeparating, TEXT("Sandbox/FightersSeparating"));
TRACE_DECLARE_INT_COUNTER(SandboxFighterSeparationQueries,
                          TEXT("Sandbox/FighterSeparationQueries"));
TRACE_DECLARE_INT_COUNTER(SandboxFighterSeparationCandidates,
                          TEXT("Sandbox/FighterSeparationCandidates"));
TRACE_DECLARE_INT_COUNTER(SandboxFighterDenseDirectionSelections,
                          TEXT("Sandbox/FighterDenseDirectionSelections"));
TRACE_DECLARE_INT_COUNTER(SandboxFighterSteeringMemory, TEXT("Sandbox/FighterSteeringMemory"));
TRACE_DECLARE_INT_COUNTER(SandboxFighterNavigationClear, TEXT("Sandbox/FighterNavigationClear"));
TRACE_DECLARE_INT_COUNTER(SandboxFighterNavigationNearby, TEXT("Sandbox/FighterNavigationNearby"));
TRACE_DECLARE_INT_COUNTER(SandboxFighterNavigationActive, TEXT("Sandbox/FighterNavigationActive"));
TRACE_DECLARE_INT_COUNTER(SandboxFighterNavigationImmediate,
                          TEXT("Sandbox/FighterNavigationImmediate"));

namespace ml::test_capital_ship_fighters {
struct Simulation::NavigationScratch {
    explicit NavigationScratch(std::pmr::memory_resource* const resource)
        : ready_fighter_indices{resource}
        , line_of_sight_starts{resource}
        , line_of_sight_ends{resource}
        , line_of_sight_results{resource}
        , trace_hits{resource}
        , blocked_fighter_indices{resource}
        , trace_fighter_indices{resource}
        , trace_choice_indices{resource}
        , observed_risk_tiers{resource} {}

    TFrameArray<int32> ready_fighter_indices;
    FFrameVectors3f line_of_sight_starts;
    FFrameVectors3f line_of_sight_ends;
    TFrameArray<uint8> line_of_sight_results;
    FFrameTraceHits trace_hits;
    TFrameArray<int32> blocked_fighter_indices;
    TFrameArray<int32> trace_fighter_indices;
    TFrameArray<int8> trace_choice_indices;
    TFrameArray<uint8> observed_risk_tiers;
};

namespace fighter_scratch {
struct FFiringScratch {
    explicit FFiringScratch(std::pmr::memory_resource* const resource)
        : new_lasers{resource}
        , aiming_dot_products{resource}
        , can_fire{resource}
        , line_of_sight_starts{resource}
        , line_of_sight_ends{resource}
        , line_of_sight_results{resource}
        , ignored_entities{resource}
        , position_fighter_indices{resource}
        , position_candidates{resource} {}

    test_lasers::FrameSpawnRequests new_lasers;
    TFrameArray<float> aiming_dot_products;
    TFrameArray<int32> can_fire;
    FFrameVectors3f line_of_sight_starts;
    FFrameVectors3f line_of_sight_ends;
    TFrameArray<uint8> line_of_sight_results;
    TFrameArray<FRegistryEntityHandle> ignored_entities;
    TFrameArray<int32> position_fighter_indices;
    FFrameVectors3f position_candidates;
};
} // namespace fighter_scratch

/* **************************************** */
// Navigation helpers
/* **************************************** */
auto Simulation::is_avoidance_direction_choice(int8 const choice) -> bool {
    return choice >= 0 && choice < n_avoidance_choices;
}
auto Simulation::make_avoidance_frame(FVector3f const preferred_direction, float const float_bias)
    -> AvoidanceFrame {
    auto const reference_axis{FMath::Abs(preferred_direction.Z) < 0.9f ? FVector3f::UpVector
                                                                       : FVector3f::RightVector};
    auto const first_lateral{
        FVector3f::CrossProduct(reference_axis, preferred_direction).GetSafeNormal()};
    auto const second_lateral{
        FVector3f::CrossProduct(preferred_direction, first_lateral).GetSafeNormal()};

    float roll_sin;
    float roll_cos;
    FMath::SinCos(&roll_sin, &roll_cos, float_bias * 2.f * UE_PI);

    return {
        preferred_direction,
        first_lateral,
        second_lateral,
        roll_sin,
        roll_cos,
    };
}
auto Simulation::make_avoidance_direction(AvoidanceFrame const& frame, int8 const choice)
    -> FVector3f {
    check(is_avoidance_direction_choice(choice));

    auto const ring{choice / 4};
    auto const ring_index{choice % 4};
    FVector3f lateral_direction;
    switch (ring_index) {
        case 0:
            lateral_direction =
                frame.first_lateral * frame.roll_cos + frame.second_lateral * frame.roll_sin;
            break;
        case 1:
            lateral_direction =
                frame.first_lateral * -frame.roll_sin + frame.second_lateral * frame.roll_cos;
            break;
        case 2:
            lateral_direction =
                frame.first_lateral * -frame.roll_cos + frame.second_lateral * -frame.roll_sin;
            break;
        default:
            lateral_direction =
                frame.first_lateral * frame.roll_sin + frame.second_lateral * -frame.roll_cos;
            break;
    }

    auto const forward_weight{ring == 0 ? sqrt_three_over_two : escape_forward_weight};
    auto const lateral_weight{ring == 0 ? half_weight : escape_lateral_weight};
    return frame.preferred_direction * forward_weight + lateral_direction * lateral_weight;
}
auto Simulation::make_avoidance_directions(AvoidanceFrame const& frame)
    -> TStaticArray<FVector3f, n_avoidance_choices> {
    TStaticArray<FVector3f, 4> const lateral_directions{
        frame.first_lateral * frame.roll_cos + frame.second_lateral * frame.roll_sin,
        frame.first_lateral * -frame.roll_sin + frame.second_lateral * frame.roll_cos,
        frame.first_lateral * -frame.roll_cos + frame.second_lateral * -frame.roll_sin,
        frame.first_lateral * frame.roll_sin + frame.second_lateral * -frame.roll_cos,
    };
    auto const shallow_forward{frame.preferred_direction * sqrt_three_over_two};
    auto const escape_forward{frame.preferred_direction * escape_forward_weight};

    TStaticArray<FVector3f, n_avoidance_choices> directions;
    for (int32 ring_index{}; ring_index < 4; ++ring_index) {
        directions[ring_index] = shallow_forward + lateral_directions[ring_index] * half_weight;
        directions[ring_index + 4] =
            escape_forward + lateral_directions[ring_index] * escape_lateral_weight;
    }
    return directions;
}
auto Simulation::make_avoidance_choice_order(uint32 const integral_bias, int8 const previous_choice)
    -> TStaticArray<int8, n_avoidance_choices> {
    TStaticArray<int8, n_avoidance_choices> result;
    int32 write_index{};
    if (is_avoidance_direction_choice(previous_choice)) {
        result[write_index++] = previous_choice;
    }

    auto const first_ring_index{static_cast<int32>(integral_bias % 4)};
    auto const direction{(integral_bias & 4u) == 0 ? 1 : -1};
    for (int32 ring{}; ring < 2; ++ring) {
        for (int32 offset{}; offset < 4; ++offset) {
            auto const ring_index{(first_ring_index + direction * offset + 4) % 4};
            auto const choice{static_cast<int8>(ring * 4 + ring_index)};
            if (choice != previous_choice) {
                result[write_index++] = choice;
            }
        }
    }
    check(write_index == n_avoidance_choices);
    return result;
}
auto Simulation::make_coincident_separation_direction(FRegistryEntityHandle const self,
                                                      FRegistryEntityHandle const other)
    -> FVector3f {
    check(self != other);
    auto const first{self < other ? self : other};
    auto const second{self < other ? other : self};
    auto const pair_hash{HashCombineFast(GetTypeHash(first), GetTypeHash(second))};
    auto const biases{ml::make_deterministic_biases(static_cast<int32>(pair_hash),
                                                    static_cast<int32>(pair_hash ^ 0x9e3779b9u))};
    auto const z{biases.floating * 2.f - 1.f};
    auto const radial{FMath::Sqrt(FMath::Max(0.f, 1.f - z * z))};
    float angle_sin;
    float angle_cos;
    FMath::SinCos(&angle_sin,
                  &angle_cos,
                  static_cast<float>(biases.integral & 0x00ffffffu) * (2.f * UE_PI / 16'777'216.f));
    FVector3f const direction{radial * angle_cos, radial * angle_sin, z};
    return self == first ? direction : -direction;
}
auto Simulation::get_navigation_tick_period(NavigationRiskTier const tier) const
    -> FPeriodicTickCountdown16::counter_type {
    auto const index{static_cast<int32>(tier)};
    check(index >= 0 && index < navigation_tick_periods.Num());
    return navigation_tick_periods[index];
}
void Simulation::update_navigation_risk(int32 const fighter_index,
                                        NavigationRiskTier const observed_tier) {
    auto& data{entity_buffers.current()};
    auto current_tier{static_cast<NavigationRiskTier>(data.navigation_risk_tiers[fighter_index])};
    auto& lower_risk_scans{data.navigation_lower_risk_scan_counts[fighter_index]};
    if (observed_tier >= current_tier) {
        current_tier = observed_tier;
        lower_risk_scans = 0;
    } else {
        ++lower_risk_scans;
        if (lower_risk_scans >= lower_risk_scans_to_demote) {
            current_tier = observed_tier;
            lower_risk_scans = 0;
        }
    }

    data.navigation_risk_tiers[fighter_index] = static_cast<uint8>(current_tier);
    auto const period{get_navigation_tick_period(current_tier)};
    data.navigation_update_countdowns.periods[fighter_index] = period;
    data.navigation_update_countdowns.remaining_ticks[fighter_index] = period;
}
void Simulation::reset_navigation_state(int32 const fighter_index,
                                        NavigationRiskTier const initial_tier) {
    auto& data{entity_buffers.current()};
    ml::assign(data.separation_steering, fighter_index, FVector3f::ZeroVector);
    data.navigation_risk_tiers[fighter_index] = static_cast<uint8>(initial_tier);
    data.navigation_lower_risk_scan_counts[fighter_index] = 0;
    data.avoidance_choice_indices[fighter_index] = direct_movement_choice;
    data.avoidance_clear_scan_counts[fighter_index] = 0;
    data.navigation_update_countdowns.periods[fighter_index] =
        get_navigation_tick_period(initial_tier);
    data.navigation_update_countdowns.remaining_ticks[fighter_index] = 0;
}

/* **************************************** */
// Combat helpers
/* **************************************** */
auto Simulation::make_fire_point_candidate(FVector3f const target_location,
                                           FVector3f const reference_location,
                                           float const fire_point_distance,
                                           float const trace_end_offset,
                                           float const desired_attack_distance,
                                           uint32 const integral_bias,
                                           float const float_bias,
                                           uint32 const candidate_order) -> FirePointCandidate {
    auto const base_direction{(reference_location - target_location).GetSafeNormal()};
    auto const base_candidate_rotation{base_direction.ToOrientationRotator()};
    auto const pattern_yaw_offset{float_bias * 360.f};
    auto const n_offsets{static_cast<uint32>(fire_point_angle_offsets.size())};
    auto const first_offset_index{integral_bias % n_offsets};
    auto const offset_index{(first_offset_index + candidate_order) % n_offsets};
    auto const angle_offset{fire_point_angle_offsets[offset_index]};
    FVector3d const trace_target{target_location};

    auto candidate_rotation{base_candidate_rotation};
    candidate_rotation.Yaw += pattern_yaw_offset + angle_offset.yaw;
    candidate_rotation.Pitch += angle_offset.pitch;

    auto const candidate_direction{candidate_rotation.Vector()};
    auto const candidate_location{target_location + candidate_direction * desired_attack_distance};
    auto const candidate_aim_direction{
        (trace_target - FVector3d{candidate_location}).GetSafeNormal()};
    auto const trace_start{FVector3d{candidate_location} +
                           candidate_aim_direction * fire_point_distance};
    auto const trace_direction{(trace_target - trace_start).GetSafeNormal()};
    auto const trace_end{trace_target - trace_direction * trace_end_offset};
    return {candidate_location, FVector3f{trace_start}, FVector3f{trace_end}};
}

/* **************************************** */
// Configuration
/* **************************************** */
void Simulation::set_config(FFighterSimulationConfig const& new_config,
                            TConstArrayView<ETestTeam> const participating_teams) noexcept {
    config = new_config;
    for (auto& is_participant : participant_mask) {
        is_participant = 0;
    }
    for (auto const team : participating_teams) {
        auto const team_index{static_cast<int32>(team)};
        if (team_index >= 0 && team_index < participant_mask.Num()) {
            participant_mask[team_index] = 1;
        }
    }

    per_team_limit = participating_teams.IsEmpty()
                       ? 0
                       : FMath::Max(0, config.max_live_fighters) / participating_teams.Num();
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
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ship_fighters::Simulation::begin_play);
    TRACE_COUNTER_SET(SandboxTestFighterCount, 0);
    TRACE_COUNTER_SET(SandboxFightersAvoiding, 0);
    TRACE_COUNTER_SET(SandboxFighterNavigationTraces, 0);
    TRACE_COUNTER_SET(SandboxFightersSeparating, 0);
    TRACE_COUNTER_SET(SandboxFighterSeparationQueries, 0);
    TRACE_COUNTER_SET(SandboxFighterSeparationCandidates, 0);
    TRACE_COUNTER_SET(SandboxFighterDenseDirectionSelections, 0);
    TRACE_COUNTER_SET(SandboxFighterSteeringMemory, 0);
    TRACE_COUNTER_SET(SandboxFighterNavigationClear, 0);
    TRACE_COUNTER_SET(SandboxFighterNavigationNearby, 0);
    TRACE_COUNTER_SET(SandboxFighterNavigationActive, 0);
    TRACE_COUNTER_SET(SandboxFighterNavigationImmediate, 0);
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

    TStaticArray<float, static_cast<int32>(NavigationRiskTier::Count)> const frequencies{
        config.avoidance_clear_update_frequency,
        config.avoidance_update_frequency,
        config.avoidance_active_update_frequency,
        config.avoidance_immediate_update_frequency,
    };
    for (int32 i{}; i < frequencies.Num(); ++i) {
        auto const period{simulation_clock.frequency_to_tick_period(frequencies[i])};
        check(FPeriodicTickCountdown16::valid_period(period));
        navigation_tick_periods[i] = static_cast<FPeriodicTickCountdown16::counter_type>(period);
    }
    auto const clear_update_interval{
        static_cast<float>(get_navigation_tick_period(NavigationRiskTier::Clear)) *
        simulation_clock.get_tick_period()};
    minimum_navigation_lookahead_time = clear_update_interval * 1.25f;

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
    data.navigation_update_countdowns.tick();
    ml::fill(data.velocities, 0.f);
    clear_tick_buffers();

    for (auto& capacity : remaining_team_capacity) {
        capacity = per_team_limit;
    }
    for (auto const team : data.teams) {
        auto const team_index{static_cast<int32>(team)};
        if (team_index >= 0 && team_index < participant_mask.Num() &&
            participant_mask[team_index] != 0) {
            remaining_team_capacity[team_index] =
                FMath::Max(0, remaining_team_capacity[team_index] - 1);
        } else {
            ensureAlwaysMsgf(
                false, TEXT("Live fighter has invalid or non-participating team %d"), team_index);
        }
    }
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
        if (entity_registry.is_valid_alive(target_handle) &&
            data.target_distance_sq[i] <= attack_engagement_threshold_sq) {
            continue;
        }

        auto const n_nearby_entities{spatial_query_manager.collect_non_team_entities_in_range(
            fighter_location, data.teams[i], awareness_radius, nearby_entities)};
        auto const aim_direction{ml::get_vector3f(data.aim_directions, i)};
        for (int32 j{0}; j < n_nearby_entities; ++j) {
            auto const potential_target{nearby_entities[j]};
            auto const potential_target_location{entity_registry.get_location(potential_target)};
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
            attack_view.desired_aiming_directions.set(i, desired_firing_direction);

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
            attack_view.target_directions.set(i, target_direction);
            attack_view.desired_move_locations.set(
                i,
                ml::get_vector3f(attack_view.target_locations, i) -
                    target_direction * desired_attack_distance);
        }
    }

    ml::direction_and_distance(
        data.movement_directions, data.move_distances, data.locations, data.desired_move_locations);
    update_navigation_steering();
    if (do_move) {
        ml::lerp_in_place(move_view.aim_directions, move_view.movement_directions, d_turn);
    }
    if (do_attack) {
        for (int32 i{}; i < n_attack; ++i) {
            auto const desired_direction{
                is_avoidance_direction_choice(attack_view.avoidance_choice_indices[i])
                    ? ml::get_vector3f(attack_view.movement_directions, i)
                    : ml::get_vector3f(attack_view.desired_aiming_directions, i)};
            auto const aim_direction{FMath::Lerp(
                ml::get_vector3f(attack_view.aim_directions, i), desired_direction, d_turn)};
            attack_view.aim_directions.set(i, aim_direction);
        }
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
    ml::batch::resolve_damage_events(entity_registry,
                                     data.entity_handles,
                                     data.healths,
                                     local_indices_to_remove,
                                     entity_death_info);

    auto const& direct_damage{entity_registry.get_direct_damage_queue_view()};
    auto const n_direct_damage{direct_damage.num()};
    for (int32 i{0}; i < n_direct_damage; ++i) {
        auto const local_index{data.entity_handles.Find(direct_damage.damaged_entities[i])};
        auto const instigator{direct_damage.instigators[i]};
        if (local_index != INDEX_NONE && entity_registry.is_valid_handle(instigator) &&
            entity_registry.get_team(instigator) != data.teams[local_index]) {
            data.target_handles[local_index] = instigator;
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
    entity_registry.queue_entity_updates(view, entity_death_info);
}
void Simulation::sync_from_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        Sandbox::test_capital_ship_fighters::Simulation::sync_from_registry);

    tasks_are_contiguous();
    remove_dead_entities();
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

/* **************************************** */
// Movement
/* **************************************** */
void Simulation::move(float const dt, TaskView const& fighters) {
    check(dt > 0.f);
    auto const n{fighters.num()};
    for (int32 i{0}; i < n; ++i) {
        auto const max_move_distance{fighters.speeds[i] * dt};
        fighters.move_distances[i] = FMath::Min(fighters.move_distances[i], max_move_distance);
        auto const velocity{ml::get_vector3f(fighters.movement_directions, i) *
                            (fighters.move_distances[i] / dt)};
        fighters.velocities.set(i, velocity);
    }

    ml::add_scaled_in_place(fighters.locations,
                            fighters.movement_directions,
                            TConstArrayView<float>{fighters.move_distances},
                            1.f);
}
void Simulation::update_navigation_steering() {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        Sandbox::test_capital_ship_fighters::Simulation::update_navigation_steering);

    auto const clearance{collision_radius + config.avoidance_clearance_buffer};
    auto const minimum_lookahead_distance{collision_radius * 2.f};
    auto const avoidance_lookahead_time{
        FMath::Max(config.avoidance_lookahead_time, minimum_navigation_lookahead_time)};
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
    scratch.ready_fighter_indices.reserve(data.num());
    scratch.observed_risk_tiers.set_num(data.num());

    auto const collect_ready_fighters{[&](Task const task) {
        auto const span{get_task_span(task)};
        for (int32 local_index{}; local_index < span.count; ++local_index) {
            auto const fighter_index{span.offset + local_index};
            if (data.navigation_update_countdowns.try_consume(fighter_index)) {
                scratch.ready_fighter_indices.add(fighter_index);
                scratch.observed_risk_tiers[fighter_index] =
                    static_cast<uint8>(NavigationRiskTier::Clear);
            }
        }
    }};
    collect_ready_fighters(Task::MoveToDestination);
    collect_ready_fighters(Task::Attack);
}
void Simulation::update_separation_observations(NavigationScratch& scratch) {
    auto& data{entity_buffers.current()};
    // Fixed capacity bounds scoring work and keeps neighbour storage off the heap.
    TStaticArray<FRegistryEntityHandle, max_separation_neighbours> nearby_fighters;
    auto const separation_radius{config.separation_radius};
    auto const immediate_distance{collision_radius * 2.f};
    auto const close_distance{FMath::Max(separation_radius * 0.5f, immediate_distance)};
    auto const immediate_distance_sq{immediate_distance * immediate_distance};
    auto const close_distance_sq{close_distance * close_distance};

    for (auto const fighter_index : scratch.ready_fighter_indices) {
        auto const goal_direction{ml::get_vector3f(data.movement_directions, fighter_index)};
        auto const move_distance{data.move_distances[fighter_index]};
        if (goal_direction.IsNearlyZero() || move_distance <= 0.f) {
            ml::assign(data.separation_steering, fighter_index, FVector3f::ZeroVector);
            data.avoidance_choice_indices[fighter_index] = direct_movement_choice;
            data.avoidance_clear_scan_counts[fighter_index] = 0;
            continue;
        }

        auto const fighter_location{ml::get_vector3f(data.locations, fighter_index)};
        auto const fighter_handle{data.entity_handles[fighter_index]};
        auto const n_nearby{spatial_query_manager.collect_entities_of_type_in_range(
            fighter_location,
            ETestEntityType::CapitalShipFighter,
            separation_radius,
            fighter_handle,
            nearby_fighters)};
        ++navigation_telemetry.separation_query_count;
        navigation_telemetry.separation_candidate_count += n_nearby;

        TStaticArray<FVector3f, max_separation_neighbours> directions_to_neighbours;
        TStaticArray<float, max_separation_neighbours> neighbour_weights;
        FVector3f separation_observation{FVector3f::ZeroVector};
        float closest_distance_sq{TNumericLimits<float>::Max()};
        for (int32 neighbour_index{}; neighbour_index < n_nearby; ++neighbour_index) {
            auto const neighbour_handle{nearby_fighters[neighbour_index]};
            auto const neighbour_location{entity_registry.get_location(neighbour_handle)};
            auto const offset{fighter_location - neighbour_location};
            auto const distance_sq{offset.SizeSquared()};
            closest_distance_sq = FMath::Min(closest_distance_sq, distance_sq);

            float distance{};
            FVector3f away_direction;
            if (distance_sq <= UE_KINDA_SMALL_NUMBER * UE_KINDA_SMALL_NUMBER) {
                away_direction =
                    make_coincident_separation_direction(fighter_handle, neighbour_handle);
            } else {
                distance = FMath::Sqrt(distance_sq);
                away_direction = offset / distance;
            }
            auto weight{1.f - FMath::Clamp(distance / separation_radius, 0.f, 1.f)};
            weight *= weight;
            separation_observation += away_direction * weight;
            directions_to_neighbours[neighbour_index] = -away_direction;
            neighbour_weights[neighbour_index] = weight;
        }

        auto const previous_memory{ml::get_vector3f(data.separation_steering, fighter_index)};
        auto const current_tier{
            static_cast<NavigationRiskTier>(data.navigation_risk_tiers[fighter_index])};
        auto const elapsed_since_scan{static_cast<float>(get_navigation_tick_period(current_tier)) *
                                      simulation_clock.get_tick_period()};
        auto const memory_retention{
            config.steering_memory_duration > 0.f
                ? FMath::Clamp(1.f - elapsed_since_scan / config.steering_memory_duration, 0.f, 1.f)
                : 0.f};
        auto const retained_memory{previous_memory * memory_retention};

        // Repulsion can cancel in a symmetric crowd. Score the same deterministic directions
        // used for hard avoidance, with retained memory favouring a consistent escape direction.
        auto const dense_traffic{n_nearby >= config.dense_traffic_neighbour_threshold &&
                                 config.separation_strength > UE_SMALL_NUMBER};
        if (dense_traffic) {
            auto const frame{
                make_avoidance_frame(goal_direction, data.float_biases[fighter_index])};
            auto const candidates{make_avoidance_directions(frame)};
            auto const memory_direction{retained_memory.GetSafeNormal()};
            auto const memory_strength{retained_memory.Size()};
            auto const candidate_score{[&](FVector3f const candidate) {
                float crowd_penalty{};
                for (int32 neighbour_index{}; neighbour_index < n_nearby; ++neighbour_index) {
                    auto const towards_dot{
                        FMath::Max(0.f,
                                   FVector3f::DotProduct(
                                       candidate, directions_to_neighbours[neighbour_index]))};
                    crowd_penalty += neighbour_weights[neighbour_index] * towards_dot * towards_dot;
                }
                return FVector3f::DotProduct(candidate, goal_direction) * crowd_goal_score_weight -
                       crowd_penalty * config.separation_strength +
                       FVector3f::DotProduct(candidate, memory_direction) * memory_strength *
                           steering_memory_score_weight;
            }};

            auto best_direction{goal_direction};
            auto best_score{candidate_score(best_direction)};
            auto const choice_order{make_avoidance_choice_order(data.integral_biases[fighter_index],
                                                                direct_movement_choice)};
            for (auto const choice : choice_order) {
                auto const candidate{candidates[choice]};
                auto const score{candidate_score(candidate)};
                if (score > best_score) {
                    best_score = score;
                    best_direction = candidate;
                }
            }

            separation_observation = (best_direction - goal_direction) / config.separation_strength;
            ++navigation_telemetry.dense_direction_selection_count;
        }

        separation_observation = separation_observation.GetClampedToMaxSize(1.f);
        auto const steering_memory{
            (retained_memory + separation_observation).GetClampedToMaxSize(1.f)};
        ml::assign(data.separation_steering, fighter_index, steering_memory);

        auto observed_tier{NavigationRiskTier::Clear};
        if (closest_distance_sq <= immediate_distance_sq) {
            observed_tier = NavigationRiskTier::Immediate;
        } else if (closest_distance_sq <= close_distance_sq) {
            observed_tier = NavigationRiskTier::Active;
        } else if (n_nearby > 0) {
            observed_tier = NavigationRiskTier::Nearby;
        }
        scratch.observed_risk_tiers[fighter_index] = static_cast<uint8>(observed_tier);
    }
}
void Simulation::apply_separation_steering() {
    auto& data{entity_buffers.current()};
    auto const apply_separation{[&](Task const task) {
        auto const span{get_task_span(task)};
        for (int32 local_index{}; local_index < span.count; ++local_index) {
            auto const fighter_index{span.offset + local_index};
            auto const separation_steering{
                ml::get_vector3f(data.separation_steering, fighter_index)};
            if (separation_steering.IsNearlyZero()) {
                continue;
            }

            auto const goal_direction{ml::get_vector3f(data.movement_directions, fighter_index)};
            auto preferred_direction{
                (goal_direction + separation_steering * config.separation_strength)
                    .GetSafeNormal()};
            if (preferred_direction.IsNearlyZero()) {
                preferred_direction = separation_steering.GetSafeNormal();
            }
            ml::assign(data.movement_directions, fighter_index, preferred_direction);
        }
    }};
    apply_separation(Task::MoveToDestination);
    apply_separation(Task::Attack);
}
void Simulation::scan_preferred_navigation(NavigationScratch& scratch,
                                           float const clearance,
                                           float const avoidance_lookahead_time,
                                           float const minimum_lookahead_distance) {
    auto& data{entity_buffers.current()};
    auto const n_ready_fighters{scratch.ready_fighter_indices.num()};
    scratch.line_of_sight_starts.reserve(n_ready_fighters);
    scratch.line_of_sight_ends.reserve(n_ready_fighters);
    scratch.trace_fighter_indices.reserve(n_ready_fighters);
    scratch.blocked_fighter_indices.reserve(n_ready_fighters);

    for (auto const fighter_index : scratch.ready_fighter_indices) {
        auto const preferred_direction{ml::get_vector3f(data.movement_directions, fighter_index)};
        auto const move_distance{data.move_distances[fighter_index]};
        if (preferred_direction.IsNearlyZero() || move_distance <= 0.f) {
            continue;
        }

        auto const lookahead_distance{
            FMath::Min(move_distance,
                       FMath::Max(data.speeds[fighter_index] * avoidance_lookahead_time,
                                  minimum_lookahead_distance))};
        auto const start{ml::get_vector3f(data.locations, fighter_index)};
        auto const end{start + preferred_direction * lookahead_distance};
        scratch.line_of_sight_starts.add(start);
        scratch.line_of_sight_ends.add(end);
        scratch.trace_fighter_indices.add(fighter_index);
    }

    auto const n_direct_traces{scratch.trace_fighter_indices.num()};
    if (n_direct_traces > 0) {
        execute_navigation_sweeps(scratch, clearance);

        for (int32 trace_index{}; trace_index < n_direct_traces; ++trace_index) {
            auto const fighter_index{scratch.trace_fighter_indices[trace_index]};
            if (scratch.line_of_sight_results[trace_index] == 0 ||
                scratch.trace_hits.hits[trace_index] != 0) {
                scratch.blocked_fighter_indices.add(fighter_index);
                data.avoidance_clear_scan_counts[fighter_index] = 0;
                continue;
            }

            auto const current_choice{data.avoidance_choice_indices[fighter_index]};
            if (current_choice == direct_movement_choice) {
                data.avoidance_clear_scan_counts[fighter_index] = 0;
                continue;
            }

            // One clear scan at an obstacle edge must not discard a held avoidance choice.
            auto& clear_scan_count{data.avoidance_clear_scan_counts[fighter_index]};
            ++clear_scan_count;
            if (clear_scan_count >= clear_scans_to_end_avoidance) {
                data.avoidance_choice_indices[fighter_index] = direct_movement_choice;
                clear_scan_count = 0;
            }
        }
    }
}
void Simulation::scan_alternative_navigation(NavigationScratch& scratch,
                                             float const clearance,
                                             float const avoidance_lookahead_time,
                                             float const minimum_lookahead_distance) {
    auto const& data{entity_buffers.current()};
    // Direct results have been consumed. Reuse their storage for eight traces per blocked fighter.
    scratch.trace_fighter_indices.clear();
    scratch.trace_choice_indices.clear();
    scratch.line_of_sight_starts.clear();
    scratch.line_of_sight_ends.clear();
    scratch.line_of_sight_results.clear();
    scratch.trace_hits.clear();

    auto const n_candidate_traces{scratch.blocked_fighter_indices.num() * n_avoidance_choices};
    scratch.line_of_sight_starts.reserve(n_candidate_traces);
    scratch.line_of_sight_ends.reserve(n_candidate_traces);
    scratch.trace_choice_indices.reserve(n_candidate_traces);

    for (auto const fighter_index : scratch.blocked_fighter_indices) {
        auto const preferred_direction{ml::get_vector3f(data.movement_directions, fighter_index)};
        auto const lookahead_distance{
            FMath::Min(data.move_distances[fighter_index],
                       FMath::Max(data.speeds[fighter_index] * avoidance_lookahead_time,
                                  minimum_lookahead_distance))};
        auto const start{ml::get_vector3f(data.locations, fighter_index)};
        auto const avoidance_frame{
            make_avoidance_frame(preferred_direction, data.float_biases[fighter_index])};
        auto const avoidance_directions{make_avoidance_directions(avoidance_frame)};
        auto const choice_order{make_avoidance_choice_order(
            data.integral_biases[fighter_index], data.avoidance_choice_indices[fighter_index])};

        for (auto const choice : choice_order) {
            auto const direction{avoidance_directions[choice]};
            auto const end{start + direction * lookahead_distance};
            scratch.line_of_sight_starts.add(start);
            scratch.line_of_sight_ends.add(end);
            scratch.trace_choice_indices.add(choice);
        }
    }

    check(scratch.line_of_sight_ends.num() == n_candidate_traces);
    execute_navigation_sweeps(scratch, clearance);
}
void Simulation::execute_navigation_sweeps(NavigationScratch& scratch, float const clearance) {
    auto const trace_count{scratch.line_of_sight_ends.num()};
    if (trace_count == 0) {
        return;
    }
    FVector3f const moving_half_extent{clearance, clearance, clearance};
    scratch.line_of_sight_results.set_num(trace_count);
    spatial_query_manager.are_spheres_in_bounds(
        scratch.line_of_sight_ends.get_const_view(), clearance, scratch.line_of_sight_results);
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
    if (fighter_diagnostics::enabled.GetValueOnGameThread() == 0) {
        diagnostic_stop_reports = 0;
    }
    auto const n_blocked_fighters{scratch.blocked_fighter_indices.num()};
    for (int32 blocked_index{}; blocked_index < n_blocked_fighters; ++blocked_index) {
        auto const fighter_index{scratch.blocked_fighter_indices[blocked_index]};
        auto const fighter_location{ml::get_vector3f(data.locations, fighter_index)};
        auto chosen_choice{stop_movement_choice};
        // Partial progress must leave room until the next active scan, including its margin.
        auto const safe_progress_distance{data.speeds[fighter_index] * safe_progress_time};
        auto best_distance_sq{safe_progress_distance * safe_progress_distance};

        auto const candidate_begin{blocked_index * n_avoidance_choices};
        auto const candidate_end{candidate_begin + n_avoidance_choices};
        for (int32 candidate_trace_index{candidate_begin}; candidate_trace_index < candidate_end;
             ++candidate_trace_index) {
            auto const choice{scratch.trace_choice_indices[candidate_trace_index]};
            if (scratch.line_of_sight_results[candidate_trace_index] == 0) {
                continue;
            }
            // Traces are already in held-choice/bias order. The first clear one wins outright.
            if (scratch.trace_hits.hits[candidate_trace_index] == 0) {
                chosen_choice = choice;
                break;
            }
            auto const hit_location{ml::get_vector3f(scratch.trace_hits.locations.get_const_view(),
                                                     candidate_trace_index)};
            auto const distance_sq{FVector3f::DistSquared(fighter_location, hit_location)};
            if (distance_sq > best_distance_sq) {
                best_distance_sq = distance_sq;
                chosen_choice = choice;
            }
        }

        data.avoidance_choice_indices[fighter_index] = chosen_choice;
        if (chosen_choice == stop_movement_choice &&
            fighter_diagnostics::take_report(diagnostic_stop_reports, 8)) {
            UE_LOG(LogSandbox,
                   Display,
                   TEXT("[FighterStop] fighterRegistryIndex=%d position=%s destination=%s "
                        "preferred=%s separation=%s clearance=%.2f safeTravel=%.2f risk=%d"),
                   data.entity_handles[fighter_index].index,
                   *fighter_location.ToString(),
                   *ml::get_vector3f(data.desired_move_locations, fighter_index).ToString(),
                   *ml::get_vector3f(data.movement_directions, fighter_index).ToString(),
                   *ml::get_vector3f(data.separation_steering, fighter_index).ToString(),
                   collision_radius + config.avoidance_clearance_buffer,
                   safe_progress_distance,
                   data.navigation_risk_tiers[fighter_index]);
            for (int32 trace_index{candidate_begin}; trace_index < candidate_end; ++trace_index) {
                UE_LOG(LogSandbox,
                       Display,
                       TEXT("[FighterStop] choice=%d end=%s inWorld=%d hit=%d "
                            "blockerRegistryIndex=%d staticIndex=%d hitDistance=%.2f"),
                       scratch.trace_choice_indices[trace_index],
                       *ml::get_vector3f(scratch.line_of_sight_ends.get_const_view(), trace_index)
                            .ToString(),
                       scratch.line_of_sight_results[trace_index],
                       scratch.trace_hits.hits[trace_index],
                       scratch.trace_hits.entities[trace_index].index,
                       scratch.trace_hits.static_geometry_indices[trace_index],
                       scratch.trace_hits.hits[trace_index]
                           ? FVector3f::Dist(
                                 fighter_location,
                                 ml::get_vector3f(scratch.trace_hits.locations.get_const_view(),
                                                  trace_index))
                           : -1.f);
            }
        }
    }
}
void Simulation::apply_navigation_choices(NavigationScratch const& scratch) {
    auto& data{entity_buffers.current()};
    for (auto const fighter_index : scratch.ready_fighter_indices) {
        auto observed_tier{
            static_cast<NavigationRiskTier>(scratch.observed_risk_tiers[fighter_index])};
        if (data.avoidance_choice_indices[fighter_index] != direct_movement_choice) {
            observed_tier = FMath::Max(observed_tier, NavigationRiskTier::Active);
        }
        update_navigation_risk(fighter_index, observed_tier);
    }

    auto const apply_held_steering{[&](Task const task) {
        auto const span{get_task_span(task)};
        for (int32 local_index{}; local_index < span.count; ++local_index) {
            auto const fighter_index{span.offset + local_index};
            if (!ml::get_vector3f(data.separation_steering, fighter_index).IsNearlyZero()) {
                ++navigation_telemetry.separating_fighter_count;
                ++navigation_telemetry.steering_memory_fighter_count;
            }
            auto const choice{data.avoidance_choice_indices[fighter_index]};
            if (is_avoidance_direction_choice(choice)) {
                auto const preferred_direction{
                    ml::get_vector3f(data.movement_directions, fighter_index)};
                auto const avoidance_frame{
                    make_avoidance_frame(preferred_direction, data.float_biases[fighter_index])};
                ml::assign(data.movement_directions,
                           fighter_index,
                           make_avoidance_direction(avoidance_frame, choice));
                ++navigation_telemetry.avoiding_fighter_count;
            } else if (choice == stop_movement_choice) {
                ml::assign(data.movement_directions, fighter_index, FVector3f::ZeroVector);
                ++navigation_telemetry.avoiding_fighter_count;
            }

            auto const tier{
                static_cast<NavigationRiskTier>(data.navigation_risk_tiers[fighter_index])};
            switch (tier) {
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
                    checkNoEntry();
                    break;
            }
        }
    }};
    apply_held_steering(Task::MoveToDestination);
    apply_held_steering(Task::Attack);
}
void Simulation::publish_navigation_telemetry() const {
    TRACE_COUNTER_SET(SandboxFightersAvoiding, navigation_telemetry.avoiding_fighter_count);
    TRACE_COUNTER_SET(SandboxFighterNavigationTraces, navigation_telemetry.hard_trace_count);
    TRACE_COUNTER_SET(SandboxFightersSeparating, navigation_telemetry.separating_fighter_count);
    TRACE_COUNTER_SET(SandboxFighterSeparationQueries, navigation_telemetry.separation_query_count);
    TRACE_COUNTER_SET(SandboxFighterSeparationCandidates,
                      navigation_telemetry.separation_candidate_count);
    TRACE_COUNTER_SET(SandboxFighterDenseDirectionSelections,
                      navigation_telemetry.dense_direction_selection_count);
    TRACE_COUNTER_SET(SandboxFighterSteeringMemory,
                      navigation_telemetry.steering_memory_fighter_count);
    TRACE_COUNTER_SET(SandboxFighterNavigationClear, navigation_telemetry.clear_risk_count);
    TRACE_COUNTER_SET(SandboxFighterNavigationNearby, navigation_telemetry.nearby_risk_count);
    TRACE_COUNTER_SET(SandboxFighterNavigationActive, navigation_telemetry.active_risk_count);
    TRACE_COUNTER_SET(SandboxFighterNavigationImmediate, navigation_telemetry.immediate_risk_count);
}

/* **************************************** */
// Accessors
/* **************************************** */
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
auto Simulation::find_index(FRegistryEntityHandle const fighter_handle) const noexcept -> int32 {
    return entity_buffers.current().entity_handles.Find(fighter_handle);
}
auto Simulation::get_task_span(Task const task) const -> FIndexSpan {
    return task_spans[std::to_underlying(task)];
}

/* **************************************** */
// Targets
/* **************************************** */
void Simulation::set_target_handle_unchecked(int32 const fighter_index,
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
    ml::dist_and_dist_sq(
        data.target_distances, data.target_distance_sq, data.locations, data.target_locations);
}

/* **************************************** */
// Tasks
/* **************************************** */
void Simulation::set_task_unchecked(int32 const index, Task const task) noexcept {
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
    auto const n{task_spans.Num()};
    auto& data{entity_buffers.current()};
    for (int32 i{0}; i < n; ++i) {
        auto const span{task_spans[i]};
        const_task_views[i] = data.get_const_view(span.offset, span.count);
        task_views[i] = data.get_view(span.offset, span.count);
    }
}

/* **************************************** */
// Entity data
/* **************************************** */
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
        ml::assign(
            registry_update_data.rotations, i, ml::get_vector3f(data.aim_directions, i).Rotation());
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

    auto const task_counts{get_task_counts()};
    int32 expected_offset{};
    for (int32 i{}; i < n_task_types; ++i) {
        auto const& span{task_spans[i]};
        if (span.offset != expected_offset || span.count != task_counts[i]) {
            return false;
        }
        expected_offset += task_counts[i];
    }
    return true;
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

/* **************************************** */
// Spawning
/* **************************************** */
auto Simulation::queue_spawns(TestCapitalShipFighterSpawnQueueConstView const new_spawns) -> int32 {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ship_fighters::Simulation::queue_spawns);
    new_spawns.validate_array_sizes();
    auto const requested_count{new_spawns.num()};
    if (requested_count == 0) {
        return 0;
    }

    auto const team{new_spawns.teams[0]};
    auto const team_index{static_cast<int32>(team)};
    if (team_index < 0 || team_index >= participant_mask.Num() ||
        participant_mask[team_index] == 0) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("Rejected fighter spawn request for invalid or non-participating team %d"),
               team_index);
        return 0;
    }

    for (auto const queued_team : new_spawns.teams) {
        if (queued_team != team) {
            UE_LOG(
                LogSandbox, Error, TEXT("Rejected fighter spawn wave containing multiple teams"));
            return 0;
        }
    }

    auto const accepted_count{FMath::Min(requested_count, remaining_team_capacity[team_index])};
    spawn_queue.append_from(new_spawns.left(accepted_count));
    remaining_team_capacity[team_index] -= accepted_count;
    return accepted_count;
}
void Simulation::commit_spawns() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ship_fighters::Simulation::commit_spawns);

    if (fighter_diagnostics::enabled.GetValueOnGameThread() == 0) {
        diagnostic_spawn_reports = 0;
    }
    ml::reset(new_spawn_entity_handles, new_spawn_entity_data);
    auto const& new_locations{spawn_queue.locations};
    auto const& new_rotations{spawn_queue.rotations};
    auto const& new_teams{spawn_queue.teams};
    auto const& new_parents{spawn_queue.parents};
    auto const& new_targets{spawn_queue.targets};
    auto& data{entity_buffers.current()};
    auto const n_cur{get_num_instances()};
    auto const n_new{ml::num(new_locations)};

    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(new_locations),
        SANDBOX_NAMED_NUM(new_rotations),
        SANDBOX_NAMED_NUM(new_teams),
        SANDBOX_NAMED_NUM(new_parents),
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
    data.parent_handles.Append(new_parents);
    data.awareness_scan_countdowns.add_zeroed(n_new);
    data.navigation_update_countdowns.add_uninitialised(n_new);
    data.separation_steering.add_zeroed(n_new);
    ml::append_n(data.navigation_risk_tiers, static_cast<uint8>(NavigationRiskTier::Nearby), n_new);
    data.navigation_lower_risk_scan_counts.AddZeroed(n_new);
    ml::append_n(data.avoidance_choice_indices, direct_movement_choice, n_new);
    data.avoidance_clear_scan_counts.AddZeroed(n_new);
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
        data.aim_directions.set(index, ml::get_vector3f(new_rotations, i));
        ml::assign_from(new_spawn_entity_data.locations, i, data.locations, index);
        ml::assign(new_spawn_entity_data.rotations,
                   i,
                   ml::get_vector3f(data.aim_directions, index).Rotation());
        new_spawn_entity_data.healths[i] = data.healths[index];
        new_spawn_entity_data.teams[i] = data.teams[index];
    }
    new_spawn_entity_data.set_all_entity_types(ETestEntityType::CapitalShipFighter);
    ml::fill(new_spawn_entity_data.velocities, 0.f);

    new_spawn_entity_handles = entity_registry.add_entities(new_spawn_entity_data.get_const_view());
    new_spawn_entity_handles.registry_handles.append_to(data.entity_handles);
    if (fighter_diagnostics::enabled.GetValueOnGameThread() != 0) {
        for (int32 i{}; i < n_new; ++i) {
            if (!fighter_diagnostics::take_report(diagnostic_spawn_reports, 64)) {
                break;
            }
            auto const index{n_cur + i};
            UE_LOG(LogSandbox,
                   Display,
                   TEXT("[FighterSpawn] Committed fighterRegistryIndex=%d parentRegistryIndex=%d "
                        "targetRegistryIndex=%d world=%s"),
                   data.entity_handles[index].index,
                   data.parent_handles[index].index,
                   data.target_handles[index].index,
                   *ml::get_vector3f(data.locations, index).ToString());
        }
    }
    data.integral_biases.AddUninitialized(n_new);
    data.float_biases.AddUninitialized(n_new);
    ml::make_deterministic_biases(
        TConstArrayView<int32>{new_spawn_entity_handles.registry_handles.registry_indices.data(),
                               n_new},
        TConstArrayView<int32>{new_spawn_entity_handles.registry_handles.generations.data(), n_new},
        TArrayView<uint32>{data.integral_biases}.Slice(n_cur, n_new),
        TArrayView<float>{data.float_biases}.Slice(n_cur, n_new));
    auto const navigation_tick_period{get_navigation_tick_period(NavigationRiskTier::Nearby)};
    // A new fighter has no validated movement direction. Scan before its first movement.
    data.navigation_update_countdowns.initialise_last(navigation_tick_period, n_new);

    validate_array_sizes();
}

/* **************************************** */
// Destruction
/* **************************************** */
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
    ml::remove_at_swap_many_sorted_desc(local_indices_to_remove, data);
    validate_array_sizes();
}

/* **************************************** */
// Combat
/* **************************************** */
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
    fighter_scratch::FFiringScratch scratch{&frame_memory_resource};
    auto& new_lasers{scratch.new_lasers};
    auto& aiming_dot_products{scratch.aiming_dot_products};
    auto& can_fire{scratch.can_fire};
    auto& line_of_sight_starts{scratch.line_of_sight_starts};
    auto& line_of_sight_ends{scratch.line_of_sight_ends};
    auto& line_of_sight_results{scratch.line_of_sight_results};
    auto& firing_ignored_entities{scratch.ignored_entities};
    auto& firing_position_fighter_indices{scratch.position_fighter_indices};
    auto& firing_position_candidates{scratch.position_candidates};

    new_lasers.set_num(n_ships);
    aiming_dot_products.set_num(n_ships);
    can_fire.reserve(n_ships);
    firing_position_fighter_indices.reserve(n_ships);
    firing_position_candidates.reserve(n_ships);
    ml::dot_product(
        aiming_dot_products.view(), data.aim_directions, data.desired_aiming_directions);

    for (int32 ship_index{0}; ship_index < n_ships; ++ship_index) {
        if (!data.attack_cooldowns.is_ready(ship_index)) {
            continue;
        }
        if (data.target_distance_sq[ship_index] > laser_max_distance_sq ||
            !data.target_handles[ship_index].is_valid() ||
            aiming_dot_products[ship_index] < aim_threshold) {
            data.attack_cooldowns.set_counter(ship_index, attack_retry_cooldown_tick_value);
            continue;
        }
        can_fire.add(ship_index);
    }

    if (can_fire.is_empty()) {
        return;
    }

    auto const n_can_fire_before_los{can_fire.num()};
    auto const los_check_buffer{config.los_check_buffer};
    line_of_sight_starts.set_num(n_can_fire_before_los);
    line_of_sight_ends.set_num(n_can_fire_before_los);
    line_of_sight_results.set_num(n_can_fire_before_los);
    firing_ignored_entities.set_num(n_can_fire_before_los);
    for (int32 i{}; i < n_can_fire_before_los; ++i) {
        auto const ship_index{can_fire[i]};
        firing_ignored_entities[i] = data.entity_handles[ship_index];
        auto const ship_location{ml::get_vector3f(data.locations, ship_index)};
        auto const direction{ml::get_vector3f(data.aim_directions, ship_index)};
        auto const start{ship_location + direction * fire_point_distance};
        auto const trace_end_offset{los_check_buffer + data.target_radii[ship_index]};
        auto const end{ml::get_vector3f(data.target_locations, ship_index) -
                       direction * trace_end_offset};
        line_of_sight_starts.set(i, start);
        line_of_sight_ends.set(i, end);
    }

    spatial_query_manager.have_clear_lines(line_of_sight_starts.get_const_view(),
                                           line_of_sight_ends.get_const_view(),
                                           line_of_sight_results,
                                           firing_ignored_entities);

    for (int32 i{n_can_fire_before_los - 1}; i >= 0; --i) {
        auto const ship_index{can_fire[i]};
        if (line_of_sight_results[i] != 0) {
            continue;
        }

        can_fire.remove_at_swap(i);
        data.attack_cooldowns.set_counter(ship_index, attack_retry_cooldown_tick_value);
        auto const fighter_location{ml::get_vector3f(data.locations, ship_index)};
        auto const desired_move_location{ml::get_vector3f(data.desired_move_locations, ship_index)};
        auto const has_arrived{FVector3f::DistSquared(fighter_location, desired_move_location) <=
                               attack_position_arrival_distance_sq};
        if (!has_arrived) {
            continue;
        }

        firing_position_fighter_indices.add(ship_index);
    }

    auto const n_fire_point_candidates{static_cast<uint32>(fire_point_angle_offsets.size())};
    for (uint32 candidate_order{};
         candidate_order < n_fire_point_candidates && !firing_position_fighter_indices.is_empty();
         ++candidate_order) {
        auto const n_fighters{firing_position_fighter_indices.num()};
        line_of_sight_starts.set_num(n_fighters);
        line_of_sight_ends.set_num(n_fighters);
        line_of_sight_results.set_num(n_fighters);
        firing_ignored_entities.set_num(n_fighters);
        firing_position_candidates.set_num(n_fighters);

        for (int32 i{}; i < n_fighters; ++i) {
            auto const ship_index{firing_position_fighter_indices[i]};
            firing_ignored_entities[i] = data.entity_handles[ship_index];
            auto const candidate{
                make_fire_point_candidate(ml::get_vector3f(data.target_locations, ship_index),
                                          ml::get_vector3f(data.desired_move_locations, ship_index),
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

        spatial_query_manager.have_clear_lines(line_of_sight_starts.get_const_view(),
                                               line_of_sight_ends.get_const_view(),
                                               line_of_sight_results,
                                               firing_ignored_entities);

        for (int32 i{n_fighters - 1}; i >= 0; --i) {
            if (line_of_sight_results[i] == 0) {
                continue;
            }

            auto const ship_index{firing_position_fighter_indices[i]};
            data.desired_move_locations.set(
                ship_index, ml::get_vector3f(firing_position_candidates.get_const_view(), i));
            firing_position_fighter_indices.remove_at_swap(i);
        }
    }

    auto const n_can_fire{can_fire.num()};
    new_lasers.set_num(n_can_fire);
    for (int32 i{0}; i < n_can_fire; ++i) {
        auto const ship_index{can_fire[i]};
        auto const ship_location{ml::get_vector3f(data.locations, ship_index)};
        auto const direction{ml::get_vector3f(data.aim_directions, ship_index)};
        new_lasers.locations.set(i, ship_location + direction * fire_point_distance);
        new_lasers.rotations.set(i, direction.ToOrientationRotator());
        new_lasers.base_velocities.set(i, ml::get_vector3f(data.velocities, ship_index));
        new_lasers.instigator_handles[i] = data.entity_handles[ship_index];
        new_lasers.sources[i] = {data.teams[ship_index], ETestEntityType::CapitalShipFighter};
        data.attack_cooldowns.restart_counter(ship_index);
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
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ship_fighters::Simulation::queue_orders);
    order_queue.append_from(queue.get_const_view());
}
void Simulation::commit_orders() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_capital_ship_fighters::Simulation::commit_orders);

    auto& data{entity_buffers.current()};
    auto const n_orders{ml::num(order_queue)};
    if (n_orders < 1) {
        return;
    }

    TFrameArray<int32> index_buffer{&frame_memory_resource};
    index_buffer.set_num(n_orders);
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
            reset_navigation_state(fighter_index,
                                   new_task == Task::Standby ? NavigationRiskTier::Clear
                                                             : NavigationRiskTier::Nearby);
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

/* **************************************** */
// Misc
/* **************************************** */
void Simulation::clear_tick_buffers() {
    ml::reset(local_indices_to_remove, entity_death_info, spawn_queue, order_queue);
}

/* **************************************** */
// Checks
/* **************************************** */
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
                   *LexToString(task),
                   *LexToString(current_task_group));
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
                                       *LexToString(static_cast<Task>(i)),
                                       *to_compact_string(task_spans[i]),
                                       *to_compact_string(checked_task_spans[i]));
        }
        UE_LOG(LogSandbox, Fatal, TEXT("%s"), *message);
    }
}
#endif
} // namespace ml::test_capital_ship_fighters
