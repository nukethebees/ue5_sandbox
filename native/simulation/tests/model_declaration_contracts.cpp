#include <ioj/sim/collision/trace_entity_filter.h>
#include <ioj/sim/levels/level_mission_mode.h>
#include <ioj/sim/levels/level_validation_error_code.h>
#include <ioj/sim/player/flight_model_enums.h>
#include <ioj/sim/sim_state.h>
#include <ioj/sim/simulation_phase.h>
// Frozen declaration contracts from the handwritten simulation model.
// Keep these expectations independent of the LispB schema.
#include <ioj/sim/attack_distance_band.h>
#include <ioj/sim/base_sim_config.h>
#include <ioj/sim/capital_death_event.h>
#include <ioj/sim/collision/grid_geometry.h>
#include <ioj/sim/collision/world_aabb.h>
#include <ioj/sim/entity_types.h>
#include <ioj/sim/fighters/navigation_records.h>
#include <ioj/sim/health_move.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/laser_source.h>
#include <ioj/sim/level_telemetry_current_state.h>
#include <ioj/sim/line_trace_result.h>
#include <ioj/sim/line_traces.h>
#include <ioj/sim/navigation_telemetry.h>
#include <ioj/sim/player/flight_model_data.h>
#include <ioj/sim/player/flight_model_runtime.h>
#include <ioj/sim/simulation_phase.h>
#include <ioj/sim/trace_hits.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <type_traits>

#define IOJ_RECORD_MEMBER_CONTRACT(Type, Member)                               \
    static_assert(offsetof(Type, Member) == offsetof(Original##Type, Member)); \
    static_assert(std::is_same_v<decltype(Type::Member), decltype(Original##Type::Member)>)

namespace ioj::sim::fighters::model_contract {
struct OriginalAvoidanceFrame {
    Vector3f preferred_direction;
    Vector3f first_lateral;
    Vector3f second_lateral;
    float roll_sin;
    float roll_cos;
};
static_assert(sizeof(AvoidanceFrame) == sizeof(OriginalAvoidanceFrame));
static_assert(alignof(AvoidanceFrame) == alignof(OriginalAvoidanceFrame));
static_assert(std::is_standard_layout_v<AvoidanceFrame> ==
              std::is_standard_layout_v<OriginalAvoidanceFrame>);
static_assert(std::is_trivially_copyable_v<AvoidanceFrame> ==
              std::is_trivially_copyable_v<OriginalAvoidanceFrame>);
static_assert(std::is_aggregate_v<AvoidanceFrame>);
static_assert(std::is_trivially_default_constructible_v<AvoidanceFrame> ==
              std::is_trivially_default_constructible_v<OriginalAvoidanceFrame>);
IOJ_RECORD_MEMBER_CONTRACT(AvoidanceFrame, preferred_direction);
IOJ_RECORD_MEMBER_CONTRACT(AvoidanceFrame, first_lateral);
IOJ_RECORD_MEMBER_CONTRACT(AvoidanceFrame, second_lateral);
IOJ_RECORD_MEMBER_CONTRACT(AvoidanceFrame, roll_sin);
IOJ_RECORD_MEMBER_CONTRACT(AvoidanceFrame, roll_cos);
}

namespace ioj::sim::fighters::model_contract {
struct OriginalNavigationRiskUpdate {
    NavigationRiskTier tier;
    std::uint8_t lower_risk_scan_count;
};
static_assert(sizeof(NavigationRiskUpdate) == sizeof(OriginalNavigationRiskUpdate));
static_assert(alignof(NavigationRiskUpdate) == alignof(OriginalNavigationRiskUpdate));
static_assert(std::is_standard_layout_v<NavigationRiskUpdate> ==
              std::is_standard_layout_v<OriginalNavigationRiskUpdate>);
static_assert(std::is_trivially_copyable_v<NavigationRiskUpdate> ==
              std::is_trivially_copyable_v<OriginalNavigationRiskUpdate>);
static_assert(std::is_aggregate_v<NavigationRiskUpdate>);
static_assert(std::is_trivially_default_constructible_v<NavigationRiskUpdate> ==
              std::is_trivially_default_constructible_v<OriginalNavigationRiskUpdate>);
IOJ_RECORD_MEMBER_CONTRACT(NavigationRiskUpdate, tier);
IOJ_RECORD_MEMBER_CONTRACT(NavigationRiskUpdate, lower_risk_scan_count);
}

namespace ioj::sim::fighters::model_contract {
struct OriginalSeparationObservationParameters {
    float separation_radius;
    float immediate_distance_squared;
    float close_distance_squared;
    float memory_retention;
    float separation_strength;
    std::int32_t dense_traffic_neighbour_threshold;
    std::uint32_t integral_bias;
    float float_bias;
};
static_assert(sizeof(SeparationObservationParameters) ==
              sizeof(OriginalSeparationObservationParameters));
static_assert(alignof(SeparationObservationParameters) ==
              alignof(OriginalSeparationObservationParameters));
static_assert(std::is_standard_layout_v<SeparationObservationParameters> ==
              std::is_standard_layout_v<OriginalSeparationObservationParameters>);
static_assert(std::is_trivially_copyable_v<SeparationObservationParameters> ==
              std::is_trivially_copyable_v<OriginalSeparationObservationParameters>);
static_assert(std::is_aggregate_v<SeparationObservationParameters>);
static_assert(std::is_trivially_default_constructible_v<SeparationObservationParameters> ==
              std::is_trivially_default_constructible_v<OriginalSeparationObservationParameters>);
IOJ_RECORD_MEMBER_CONTRACT(SeparationObservationParameters, separation_radius);
IOJ_RECORD_MEMBER_CONTRACT(SeparationObservationParameters, immediate_distance_squared);
IOJ_RECORD_MEMBER_CONTRACT(SeparationObservationParameters, close_distance_squared);
IOJ_RECORD_MEMBER_CONTRACT(SeparationObservationParameters, memory_retention);
IOJ_RECORD_MEMBER_CONTRACT(SeparationObservationParameters, separation_strength);
IOJ_RECORD_MEMBER_CONTRACT(SeparationObservationParameters, dense_traffic_neighbour_threshold);
IOJ_RECORD_MEMBER_CONTRACT(SeparationObservationParameters, integral_bias);
IOJ_RECORD_MEMBER_CONTRACT(SeparationObservationParameters, float_bias);
}

namespace ioj::sim::fighters::model_contract {
struct OriginalSeparationObservation {
    Vector3f steering_memory;
    NavigationRiskTier risk_tier;
    bool dense_direction_selected;
};
static_assert(sizeof(SeparationObservation) == sizeof(OriginalSeparationObservation));
static_assert(alignof(SeparationObservation) == alignof(OriginalSeparationObservation));
static_assert(std::is_standard_layout_v<SeparationObservation> ==
              std::is_standard_layout_v<OriginalSeparationObservation>);
static_assert(std::is_trivially_copyable_v<SeparationObservation> ==
              std::is_trivially_copyable_v<OriginalSeparationObservation>);
static_assert(std::is_aggregate_v<SeparationObservation>);
static_assert(std::is_trivially_default_constructible_v<SeparationObservation> ==
              std::is_trivially_default_constructible_v<OriginalSeparationObservation>);
IOJ_RECORD_MEMBER_CONTRACT(SeparationObservation, steering_memory);
IOJ_RECORD_MEMBER_CONTRACT(SeparationObservation, risk_tier);
IOJ_RECORD_MEMBER_CONTRACT(SeparationObservation, dense_direction_selected);
}

namespace ioj::sim::fighters::model_contract {
struct OriginalSeparationNeighbour {
    EntityUniqueId id;
    Vector3f location;
};
static_assert(sizeof(SeparationNeighbour) == sizeof(OriginalSeparationNeighbour));
static_assert(alignof(SeparationNeighbour) == alignof(OriginalSeparationNeighbour));
static_assert(std::is_standard_layout_v<SeparationNeighbour> ==
              std::is_standard_layout_v<OriginalSeparationNeighbour>);
static_assert(std::is_trivially_copyable_v<SeparationNeighbour> ==
              std::is_trivially_copyable_v<OriginalSeparationNeighbour>);
static_assert(std::is_aggregate_v<SeparationNeighbour>);
static_assert(std::is_trivially_default_constructible_v<SeparationNeighbour> ==
              std::is_trivially_default_constructible_v<OriginalSeparationNeighbour>);
IOJ_RECORD_MEMBER_CONTRACT(SeparationNeighbour, id);
IOJ_RECORD_MEMBER_CONTRACT(SeparationNeighbour, location);
}

namespace ioj::sim::collision::model_contract {
struct OriginalCellCoord {
    int x{};
    int y{};
    int z{};
};
static_assert(sizeof(CellCoord) == sizeof(OriginalCellCoord));
static_assert(alignof(CellCoord) == alignof(OriginalCellCoord));
static_assert(std::is_standard_layout_v<CellCoord> == std::is_standard_layout_v<OriginalCellCoord>);
static_assert(std::is_trivially_copyable_v<CellCoord> ==
              std::is_trivially_copyable_v<OriginalCellCoord>);
static_assert(std::is_aggregate_v<CellCoord>);
static_assert(std::is_trivially_default_constructible_v<CellCoord> ==
              std::is_trivially_default_constructible_v<OriginalCellCoord>);
IOJ_RECORD_MEMBER_CONTRACT(CellCoord, x);
IOJ_RECORD_MEMBER_CONTRACT(CellCoord, y);
IOJ_RECORD_MEMBER_CONTRACT(CellCoord, z);
TEST(SimulationModelDefaults, CellCoord) {
    CellCoord const value{};
    EXPECT_EQ(value.x, (int{}));
    EXPECT_EQ(value.y, (int{}));
    EXPECT_EQ(value.z, (int{}));
}
}

namespace ioj::sim::collision::model_contract {
struct OriginalCellCoordBounds {
    CellCoord min;
    CellCoord max;
};
static_assert(sizeof(CellCoordBounds) == sizeof(OriginalCellCoordBounds));
static_assert(alignof(CellCoordBounds) == alignof(OriginalCellCoordBounds));
static_assert(std::is_standard_layout_v<CellCoordBounds> ==
              std::is_standard_layout_v<OriginalCellCoordBounds>);
static_assert(std::is_trivially_copyable_v<CellCoordBounds> ==
              std::is_trivially_copyable_v<OriginalCellCoordBounds>);
static_assert(std::is_aggregate_v<CellCoordBounds>);
static_assert(std::is_trivially_default_constructible_v<CellCoordBounds> ==
              std::is_trivially_default_constructible_v<OriginalCellCoordBounds>);
IOJ_RECORD_MEMBER_CONTRACT(CellCoordBounds, min);
IOJ_RECORD_MEMBER_CONTRACT(CellCoordBounds, max);
}

namespace ioj::sim::collision::model_contract {
struct OriginalGridGeometry {
    CellCoord dimensions;
    Vector3f cell_dimensions;
};
static_assert(sizeof(GridGeometry) == sizeof(OriginalGridGeometry));
static_assert(alignof(GridGeometry) == alignof(OriginalGridGeometry));
static_assert(std::is_standard_layout_v<GridGeometry> ==
              std::is_standard_layout_v<OriginalGridGeometry>);
static_assert(std::is_trivially_copyable_v<GridGeometry> ==
              std::is_trivially_copyable_v<OriginalGridGeometry>);
static_assert(std::is_aggregate_v<GridGeometry>);
static_assert(std::is_trivially_default_constructible_v<GridGeometry> ==
              std::is_trivially_default_constructible_v<OriginalGridGeometry>);
IOJ_RECORD_MEMBER_CONTRACT(GridGeometry, dimensions);
IOJ_RECORD_MEMBER_CONTRACT(GridGeometry, cell_dimensions);
}

namespace ioj::sim::collision::model_contract {
struct OriginalWorldAABB {
    Vector3f min;
    Vector3f max;
};
static_assert(sizeof(WorldAABB) == sizeof(OriginalWorldAABB));
static_assert(alignof(WorldAABB) == alignof(OriginalWorldAABB));
static_assert(std::is_standard_layout_v<WorldAABB> == std::is_standard_layout_v<OriginalWorldAABB>);
static_assert(std::is_trivially_copyable_v<WorldAABB> ==
              std::is_trivially_copyable_v<OriginalWorldAABB>);
static_assert(std::is_aggregate_v<WorldAABB>);
static_assert(std::is_trivially_default_constructible_v<WorldAABB> ==
              std::is_trivially_default_constructible_v<OriginalWorldAABB>);
IOJ_RECORD_MEMBER_CONTRACT(WorldAABB, min);
IOJ_RECORD_MEMBER_CONTRACT(WorldAABB, max);
}

namespace ioj::sim::model_contract {
struct OriginalAttackDistanceBand {
    float minimum_ratio{0.4f};
    float desired_ratio{0.5f};
    float maximum_ratio{0.6f};
};
static_assert(sizeof(AttackDistanceBand) == sizeof(OriginalAttackDistanceBand));
static_assert(alignof(AttackDistanceBand) == alignof(OriginalAttackDistanceBand));
static_assert(std::is_standard_layout_v<AttackDistanceBand> ==
              std::is_standard_layout_v<OriginalAttackDistanceBand>);
static_assert(std::is_trivially_copyable_v<AttackDistanceBand> ==
              std::is_trivially_copyable_v<OriginalAttackDistanceBand>);
static_assert(std::is_aggregate_v<AttackDistanceBand>);
static_assert(std::is_trivially_default_constructible_v<AttackDistanceBand> ==
              std::is_trivially_default_constructible_v<OriginalAttackDistanceBand>);
IOJ_RECORD_MEMBER_CONTRACT(AttackDistanceBand, minimum_ratio);
IOJ_RECORD_MEMBER_CONTRACT(AttackDistanceBand, desired_ratio);
IOJ_RECORD_MEMBER_CONTRACT(AttackDistanceBand, maximum_ratio);
TEST(SimulationModelDefaults, AttackDistanceBand) {
    AttackDistanceBand const value{};
    EXPECT_EQ(value.minimum_ratio, (float{0.4f}));
    EXPECT_EQ(value.desired_ratio, (float{0.5f}));
    EXPECT_EQ(value.maximum_ratio, (float{0.6f}));
}
}

namespace ioj::sim::model_contract {
struct OriginalLaserWeaponSimConfig {
    std::int32_t damage{5};
    float projectile_speed{10000.f};
    float max_distance{10000.f};
    float fire_cooldown{0.33f};
};
static_assert(sizeof(LaserWeaponSimConfig) == sizeof(OriginalLaserWeaponSimConfig));
static_assert(alignof(LaserWeaponSimConfig) == alignof(OriginalLaserWeaponSimConfig));
static_assert(std::is_standard_layout_v<LaserWeaponSimConfig> ==
              std::is_standard_layout_v<OriginalLaserWeaponSimConfig>);
static_assert(std::is_trivially_copyable_v<LaserWeaponSimConfig> ==
              std::is_trivially_copyable_v<OriginalLaserWeaponSimConfig>);
static_assert(std::is_aggregate_v<LaserWeaponSimConfig>);
static_assert(std::is_trivially_default_constructible_v<LaserWeaponSimConfig> ==
              std::is_trivially_default_constructible_v<OriginalLaserWeaponSimConfig>);
IOJ_RECORD_MEMBER_CONTRACT(LaserWeaponSimConfig, damage);
IOJ_RECORD_MEMBER_CONTRACT(LaserWeaponSimConfig, projectile_speed);
IOJ_RECORD_MEMBER_CONTRACT(LaserWeaponSimConfig, max_distance);
IOJ_RECORD_MEMBER_CONTRACT(LaserWeaponSimConfig, fire_cooldown);
TEST(SimulationModelDefaults, LaserWeaponSimConfig) {
    LaserWeaponSimConfig const value{};
    EXPECT_EQ(value.damage, (std::int32_t{5}));
    EXPECT_EQ(value.projectile_speed, (float{10000.f}));
    EXPECT_EQ(value.max_distance, (float{10000.f}));
    EXPECT_EQ(value.fire_cooldown, (float{0.33f}));
}
}

namespace ioj::sim::model_contract {
struct OriginalLaserSimConfig {
    std::int32_t n_preallocated_instances{5000};
    std::int32_t collision_jobs{8};
};
static_assert(sizeof(LaserSimConfig) == sizeof(OriginalLaserSimConfig));
static_assert(alignof(LaserSimConfig) == alignof(OriginalLaserSimConfig));
static_assert(std::is_standard_layout_v<LaserSimConfig> ==
              std::is_standard_layout_v<OriginalLaserSimConfig>);
static_assert(std::is_trivially_copyable_v<LaserSimConfig> ==
              std::is_trivially_copyable_v<OriginalLaserSimConfig>);
static_assert(std::is_aggregate_v<LaserSimConfig>);
static_assert(std::is_trivially_default_constructible_v<LaserSimConfig> ==
              std::is_trivially_default_constructible_v<OriginalLaserSimConfig>);
IOJ_RECORD_MEMBER_CONTRACT(LaserSimConfig, n_preallocated_instances);
IOJ_RECORD_MEMBER_CONTRACT(LaserSimConfig, collision_jobs);
TEST(SimulationModelDefaults, LaserSimConfig) {
    LaserSimConfig const value{};
    EXPECT_EQ(value.n_preallocated_instances, (std::int32_t{5000}));
    EXPECT_EQ(value.collision_jobs, (std::int32_t{8}));
}
}

namespace ioj::sim::model_contract {
struct OriginalOverlapResponseConfig {
    std::int32_t damage_per_overlap_detection{50};
};
static_assert(sizeof(OverlapResponseConfig) == sizeof(OriginalOverlapResponseConfig));
static_assert(alignof(OverlapResponseConfig) == alignof(OriginalOverlapResponseConfig));
static_assert(std::is_standard_layout_v<OverlapResponseConfig> ==
              std::is_standard_layout_v<OriginalOverlapResponseConfig>);
static_assert(std::is_trivially_copyable_v<OverlapResponseConfig> ==
              std::is_trivially_copyable_v<OriginalOverlapResponseConfig>);
static_assert(std::is_aggregate_v<OverlapResponseConfig>);
static_assert(std::is_trivially_default_constructible_v<OverlapResponseConfig> ==
              std::is_trivially_default_constructible_v<OriginalOverlapResponseConfig>);
IOJ_RECORD_MEMBER_CONTRACT(OverlapResponseConfig, damage_per_overlap_detection);
TEST(SimulationModelDefaults, OverlapResponseConfig) {
    OverlapResponseConfig const value{};
    EXPECT_EQ(value.damage_per_overlap_detection, (std::int32_t{50}));
}
}

namespace ioj::sim::model_contract {
struct OriginalFighterSimConfig {
    std::int32_t max_live_fighters{2000};
    float fire_dot_product_threshold{0.95f};
    float speed{2000.f};
    float turn_speed_unitless{1.f};
    float avoidance_clear_update_frequency{2.f};
    float avoidance_update_frequency{5.f};
    float avoidance_active_update_frequency{12.f};
    float avoidance_immediate_update_frequency{30.f};
    float avoidance_lookahead_time{1.f};
    float avoidance_clearance_buffer{100.f};
    float separation_radius{2000.f};
    float separation_strength{1.f};
    float steering_memory_duration{0.75f};
    std::int32_t dense_traffic_neighbour_threshold{4};
    LaserWeaponSimConfig laser{};
    Health health{50};
    float attack_retry_cooldown{0.15f};
    float attack_engagement_threshold{5000.f};
    float attack_reposition_frequency{10.f};
    AttackDistanceBand attack_distance_band;
    float arrival_distance{500.f};
    float los_check_buffer{100.f};
    float awareness_radius{10000.f};
    float awareness_scan_frequency{6.f};
    float minimum_opportunistic_intercept_deviation_dot_product{0.5f};
};
static_assert(sizeof(FighterSimConfig) == sizeof(OriginalFighterSimConfig));
static_assert(alignof(FighterSimConfig) == alignof(OriginalFighterSimConfig));
static_assert(std::is_standard_layout_v<FighterSimConfig> ==
              std::is_standard_layout_v<OriginalFighterSimConfig>);
static_assert(std::is_trivially_copyable_v<FighterSimConfig> ==
              std::is_trivially_copyable_v<OriginalFighterSimConfig>);
static_assert(std::is_aggregate_v<FighterSimConfig>);
static_assert(std::is_trivially_default_constructible_v<FighterSimConfig> ==
              std::is_trivially_default_constructible_v<OriginalFighterSimConfig>);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, max_live_fighters);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, fire_dot_product_threshold);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, speed);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, turn_speed_unitless);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, avoidance_clear_update_frequency);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, avoidance_update_frequency);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, avoidance_active_update_frequency);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, avoidance_immediate_update_frequency);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, avoidance_lookahead_time);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, avoidance_clearance_buffer);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, separation_radius);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, separation_strength);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, steering_memory_duration);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, dense_traffic_neighbour_threshold);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, laser);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, health);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, attack_retry_cooldown);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, attack_engagement_threshold);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, attack_reposition_frequency);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, attack_distance_band);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, arrival_distance);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, los_check_buffer);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, awareness_radius);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, awareness_scan_frequency);
IOJ_RECORD_MEMBER_CONTRACT(FighterSimConfig, minimum_opportunistic_intercept_deviation_dot_product);
TEST(SimulationModelDefaults, FighterSimConfig) {
    FighterSimConfig const value{};
    EXPECT_EQ(value.max_live_fighters, (std::int32_t{2000}));
    EXPECT_EQ(value.fire_dot_product_threshold, (float{0.95f}));
    EXPECT_EQ(value.speed, (float{2000.f}));
    EXPECT_EQ(value.turn_speed_unitless, (float{1.f}));
    EXPECT_EQ(value.avoidance_clear_update_frequency, (float{2.f}));
    EXPECT_EQ(value.avoidance_update_frequency, (float{5.f}));
    EXPECT_EQ(value.avoidance_active_update_frequency, (float{12.f}));
    EXPECT_EQ(value.avoidance_immediate_update_frequency, (float{30.f}));
    EXPECT_EQ(value.avoidance_lookahead_time, (float{1.f}));
    EXPECT_EQ(value.avoidance_clearance_buffer, (float{100.f}));
    EXPECT_EQ(value.separation_radius, (float{2000.f}));
    EXPECT_EQ(value.separation_strength, (float{1.f}));
    EXPECT_EQ(value.steering_memory_duration, (float{0.75f}));
    EXPECT_EQ(value.dense_traffic_neighbour_threshold, (std::int32_t{4}));
    EXPECT_EQ(value.health, (Health{50}));
    EXPECT_EQ(value.attack_retry_cooldown, (float{0.15f}));
    EXPECT_EQ(value.attack_engagement_threshold, (float{5000.f}));
    EXPECT_EQ(value.attack_reposition_frequency, (float{10.f}));
    EXPECT_EQ(value.arrival_distance, (float{500.f}));
    EXPECT_EQ(value.los_check_buffer, (float{100.f}));
    EXPECT_EQ(value.awareness_radius, (float{10000.f}));
    EXPECT_EQ(value.awareness_scan_frequency, (float{6.f}));
    EXPECT_EQ(value.minimum_opportunistic_intercept_deviation_dot_product, (float{0.5f}));
}
}

namespace ioj::sim::player::model_contract {
struct OriginalRateLimitedResponseConfig {
    float increasing_rate{};
    float decreasing_rate{};
};
static_assert(sizeof(RateLimitedResponseConfig) == sizeof(OriginalRateLimitedResponseConfig));
static_assert(alignof(RateLimitedResponseConfig) == alignof(OriginalRateLimitedResponseConfig));
static_assert(std::is_standard_layout_v<RateLimitedResponseConfig> ==
              std::is_standard_layout_v<OriginalRateLimitedResponseConfig>);
static_assert(std::is_trivially_copyable_v<RateLimitedResponseConfig> ==
              std::is_trivially_copyable_v<OriginalRateLimitedResponseConfig>);
static_assert(std::is_aggregate_v<RateLimitedResponseConfig>);
static_assert(std::is_trivially_default_constructible_v<RateLimitedResponseConfig> ==
              std::is_trivially_default_constructible_v<OriginalRateLimitedResponseConfig>);
IOJ_RECORD_MEMBER_CONTRACT(RateLimitedResponseConfig, increasing_rate);
IOJ_RECORD_MEMBER_CONTRACT(RateLimitedResponseConfig, decreasing_rate);
TEST(SimulationModelDefaults, RateLimitedResponseConfig) {
    RateLimitedResponseConfig const value{};
    EXPECT_EQ(value.increasing_rate, (float{}));
    EXPECT_EQ(value.decreasing_rate, (float{}));
}
}

namespace ioj::sim::player::model_contract {
struct OriginalSecondOrderResponseConfig {
    float settling_time{3.f};
    float damping_ratio{0.5f};
};
static_assert(sizeof(SecondOrderResponseConfig) == sizeof(OriginalSecondOrderResponseConfig));
static_assert(alignof(SecondOrderResponseConfig) == alignof(OriginalSecondOrderResponseConfig));
static_assert(std::is_standard_layout_v<SecondOrderResponseConfig> ==
              std::is_standard_layout_v<OriginalSecondOrderResponseConfig>);
static_assert(std::is_trivially_copyable_v<SecondOrderResponseConfig> ==
              std::is_trivially_copyable_v<OriginalSecondOrderResponseConfig>);
static_assert(std::is_aggregate_v<SecondOrderResponseConfig>);
static_assert(std::is_trivially_default_constructible_v<SecondOrderResponseConfig> ==
              std::is_trivially_default_constructible_v<OriginalSecondOrderResponseConfig>);
IOJ_RECORD_MEMBER_CONTRACT(SecondOrderResponseConfig, settling_time);
IOJ_RECORD_MEMBER_CONTRACT(SecondOrderResponseConfig, damping_ratio);
TEST(SimulationModelDefaults, SecondOrderResponseConfig) {
    SecondOrderResponseConfig const value{};
    EXPECT_EQ(value.settling_time, (float{3.f}));
    EXPECT_EQ(value.damping_ratio, (float{0.5f}));
}
}

namespace ioj::sim::player::model_contract {
struct OriginalResponseConfig {
    ResponseMode mode{ResponseMode::Direct};
    RateLimitedResponseConfig rate_limited{};
    SecondOrderResponseConfig second_order{};
};
static_assert(sizeof(ResponseConfig) == sizeof(OriginalResponseConfig));
static_assert(alignof(ResponseConfig) == alignof(OriginalResponseConfig));
static_assert(std::is_standard_layout_v<ResponseConfig> ==
              std::is_standard_layout_v<OriginalResponseConfig>);
static_assert(std::is_trivially_copyable_v<ResponseConfig> ==
              std::is_trivially_copyable_v<OriginalResponseConfig>);
static_assert(std::is_aggregate_v<ResponseConfig>);
static_assert(std::is_trivially_default_constructible_v<ResponseConfig> ==
              std::is_trivially_default_constructible_v<OriginalResponseConfig>);
IOJ_RECORD_MEMBER_CONTRACT(ResponseConfig, mode);
IOJ_RECORD_MEMBER_CONTRACT(ResponseConfig, rate_limited);
IOJ_RECORD_MEMBER_CONTRACT(ResponseConfig, second_order);
TEST(SimulationModelDefaults, ResponseConfig) {
    ResponseConfig const value{};
    EXPECT_EQ(value.mode, (ResponseMode{ResponseMode::Direct}));
}
}

namespace ioj::sim::player::model_contract {
struct OriginalTranslationChannelConfig {
    TranslationSemantic semantic{TranslationSemantic::Disabled};
    ReferenceFrame reference_frame{ReferenceFrame::Ship};
    TranslationInputSource input_source{TranslationInputSource::Axis};
    float automatic_value{};
    ResponseConfig response{};
};
static_assert(sizeof(TranslationChannelConfig) == sizeof(OriginalTranslationChannelConfig));
static_assert(alignof(TranslationChannelConfig) == alignof(OriginalTranslationChannelConfig));
static_assert(std::is_standard_layout_v<TranslationChannelConfig> ==
              std::is_standard_layout_v<OriginalTranslationChannelConfig>);
static_assert(std::is_trivially_copyable_v<TranslationChannelConfig> ==
              std::is_trivially_copyable_v<OriginalTranslationChannelConfig>);
static_assert(std::is_aggregate_v<TranslationChannelConfig>);
static_assert(std::is_trivially_default_constructible_v<TranslationChannelConfig> ==
              std::is_trivially_default_constructible_v<OriginalTranslationChannelConfig>);
IOJ_RECORD_MEMBER_CONTRACT(TranslationChannelConfig, semantic);
IOJ_RECORD_MEMBER_CONTRACT(TranslationChannelConfig, reference_frame);
IOJ_RECORD_MEMBER_CONTRACT(TranslationChannelConfig, input_source);
IOJ_RECORD_MEMBER_CONTRACT(TranslationChannelConfig, automatic_value);
IOJ_RECORD_MEMBER_CONTRACT(TranslationChannelConfig, response);
TEST(SimulationModelDefaults, TranslationChannelConfig) {
    TranslationChannelConfig const value{};
    EXPECT_EQ(value.semantic, (TranslationSemantic{TranslationSemantic::Disabled}));
    EXPECT_EQ(value.reference_frame, (ReferenceFrame{ReferenceFrame::Ship}));
    EXPECT_EQ(value.input_source, (TranslationInputSource{TranslationInputSource::Axis}));
    EXPECT_EQ(value.automatic_value, (float{}));
}
}

namespace ioj::sim::player::model_contract {
struct OriginalTranslationDriveConfig {
    float positive_target_speed{};
    float negative_target_speed{};
    float positive_speed_limit{effectively_unlimited_speed};
    float negative_speed_limit{effectively_unlimited_speed};
    float positive_acceleration{};
    float negative_acceleration{};
};
static_assert(sizeof(TranslationDriveConfig) == sizeof(OriginalTranslationDriveConfig));
static_assert(alignof(TranslationDriveConfig) == alignof(OriginalTranslationDriveConfig));
static_assert(std::is_standard_layout_v<TranslationDriveConfig> ==
              std::is_standard_layout_v<OriginalTranslationDriveConfig>);
static_assert(std::is_trivially_copyable_v<TranslationDriveConfig> ==
              std::is_trivially_copyable_v<OriginalTranslationDriveConfig>);
static_assert(std::is_aggregate_v<TranslationDriveConfig>);
static_assert(std::is_trivially_default_constructible_v<TranslationDriveConfig> ==
              std::is_trivially_default_constructible_v<OriginalTranslationDriveConfig>);
IOJ_RECORD_MEMBER_CONTRACT(TranslationDriveConfig, positive_target_speed);
IOJ_RECORD_MEMBER_CONTRACT(TranslationDriveConfig, negative_target_speed);
IOJ_RECORD_MEMBER_CONTRACT(TranslationDriveConfig, positive_speed_limit);
IOJ_RECORD_MEMBER_CONTRACT(TranslationDriveConfig, negative_speed_limit);
IOJ_RECORD_MEMBER_CONTRACT(TranslationDriveConfig, positive_acceleration);
IOJ_RECORD_MEMBER_CONTRACT(TranslationDriveConfig, negative_acceleration);
TEST(SimulationModelDefaults, TranslationDriveConfig) {
    TranslationDriveConfig const value{};
    EXPECT_EQ(value.positive_target_speed, (float{}));
    EXPECT_EQ(value.negative_target_speed, (float{}));
    EXPECT_EQ(value.positive_speed_limit, (float{effectively_unlimited_speed}));
    EXPECT_EQ(value.negative_speed_limit, (float{effectively_unlimited_speed}));
    EXPECT_EQ(value.positive_acceleration, (float{}));
    EXPECT_EQ(value.negative_acceleration, (float{}));
}
}

namespace ioj::sim::player::model_contract {
struct OriginalTranslationAxisConfig {
    TranslationChannelConfig manual{};
    TranslationChannelConfig automatic{};
    TranslationDriveConfig normal{};
    TranslationDriveConfig boosted{};
    float passive_drag{};
    ReferenceFrame passive_drag_reference_frame{ReferenceFrame::Ship};
    float active_stabilization_rate{};
    ReferenceFrame active_stabilization_reference_frame{ReferenceFrame::Ship};
};
static_assert(sizeof(TranslationAxisConfig) == sizeof(OriginalTranslationAxisConfig));
static_assert(alignof(TranslationAxisConfig) == alignof(OriginalTranslationAxisConfig));
static_assert(std::is_standard_layout_v<TranslationAxisConfig> ==
              std::is_standard_layout_v<OriginalTranslationAxisConfig>);
static_assert(std::is_trivially_copyable_v<TranslationAxisConfig> ==
              std::is_trivially_copyable_v<OriginalTranslationAxisConfig>);
static_assert(std::is_aggregate_v<TranslationAxisConfig>);
static_assert(std::is_trivially_default_constructible_v<TranslationAxisConfig> ==
              std::is_trivially_default_constructible_v<OriginalTranslationAxisConfig>);
IOJ_RECORD_MEMBER_CONTRACT(TranslationAxisConfig, manual);
IOJ_RECORD_MEMBER_CONTRACT(TranslationAxisConfig, automatic);
IOJ_RECORD_MEMBER_CONTRACT(TranslationAxisConfig, normal);
IOJ_RECORD_MEMBER_CONTRACT(TranslationAxisConfig, boosted);
IOJ_RECORD_MEMBER_CONTRACT(TranslationAxisConfig, passive_drag);
IOJ_RECORD_MEMBER_CONTRACT(TranslationAxisConfig, passive_drag_reference_frame);
IOJ_RECORD_MEMBER_CONTRACT(TranslationAxisConfig, active_stabilization_rate);
IOJ_RECORD_MEMBER_CONTRACT(TranslationAxisConfig, active_stabilization_reference_frame);
TEST(SimulationModelDefaults, TranslationAxisConfig) {
    TranslationAxisConfig const value{};
    EXPECT_EQ(value.passive_drag, (float{}));
    EXPECT_EQ(value.passive_drag_reference_frame, (ReferenceFrame{ReferenceFrame::Ship}));
    EXPECT_EQ(value.active_stabilization_rate, (float{}));
    EXPECT_EQ(value.active_stabilization_reference_frame, (ReferenceFrame{ReferenceFrame::Ship}));
}
}

namespace ioj::sim::player::model_contract {
struct OriginalTranslationAxesConfig {
    TranslationAxisConfig forward{};
    TranslationAxisConfig right{};
    TranslationAxisConfig up{};
};
static_assert(sizeof(TranslationAxesConfig) == sizeof(OriginalTranslationAxesConfig));
static_assert(alignof(TranslationAxesConfig) == alignof(OriginalTranslationAxesConfig));
static_assert(std::is_standard_layout_v<TranslationAxesConfig> ==
              std::is_standard_layout_v<OriginalTranslationAxesConfig>);
static_assert(std::is_trivially_copyable_v<TranslationAxesConfig> ==
              std::is_trivially_copyable_v<OriginalTranslationAxesConfig>);
static_assert(std::is_aggregate_v<TranslationAxesConfig>);
static_assert(std::is_trivially_default_constructible_v<TranslationAxesConfig> ==
              std::is_trivially_default_constructible_v<OriginalTranslationAxesConfig>);
IOJ_RECORD_MEMBER_CONTRACT(TranslationAxesConfig, forward);
IOJ_RECORD_MEMBER_CONTRACT(TranslationAxesConfig, right);
IOJ_RECORD_MEMBER_CONTRACT(TranslationAxesConfig, up);
}

namespace ioj::sim::player::model_contract {
struct OriginalRotationStabilizationConfig {
    bool enabled{};
    float target_angle{};
    float delay{};
    ResponseConfig response{};
};
static_assert(sizeof(RotationStabilizationConfig) == sizeof(OriginalRotationStabilizationConfig));
static_assert(alignof(RotationStabilizationConfig) == alignof(OriginalRotationStabilizationConfig));
static_assert(std::is_standard_layout_v<RotationStabilizationConfig> ==
              std::is_standard_layout_v<OriginalRotationStabilizationConfig>);
static_assert(std::is_trivially_copyable_v<RotationStabilizationConfig> ==
              std::is_trivially_copyable_v<OriginalRotationStabilizationConfig>);
static_assert(std::is_aggregate_v<RotationStabilizationConfig>);
static_assert(std::is_trivially_default_constructible_v<RotationStabilizationConfig> ==
              std::is_trivially_default_constructible_v<OriginalRotationStabilizationConfig>);
IOJ_RECORD_MEMBER_CONTRACT(RotationStabilizationConfig, enabled);
IOJ_RECORD_MEMBER_CONTRACT(RotationStabilizationConfig, target_angle);
IOJ_RECORD_MEMBER_CONTRACT(RotationStabilizationConfig, delay);
IOJ_RECORD_MEMBER_CONTRACT(RotationStabilizationConfig, response);
TEST(SimulationModelDefaults, RotationStabilizationConfig) {
    RotationStabilizationConfig const value{};
    EXPECT_EQ(value.enabled, (bool{}));
    EXPECT_EQ(value.target_angle, (float{}));
    EXPECT_EQ(value.delay, (float{}));
}
}

namespace ioj::sim::player::model_contract {
struct OriginalRotationAxisConfig {
    RotationSemantic manual_semantic{RotationSemantic::Disabled};
    float maximum_rate{};
    float acceleration{};
    ResponseConfig response{};
    RotationStabilizationConfig stabilization{};
};
static_assert(sizeof(RotationAxisConfig) == sizeof(OriginalRotationAxisConfig));
static_assert(alignof(RotationAxisConfig) == alignof(OriginalRotationAxisConfig));
static_assert(std::is_standard_layout_v<RotationAxisConfig> ==
              std::is_standard_layout_v<OriginalRotationAxisConfig>);
static_assert(std::is_trivially_copyable_v<RotationAxisConfig> ==
              std::is_trivially_copyable_v<OriginalRotationAxisConfig>);
static_assert(std::is_aggregate_v<RotationAxisConfig>);
static_assert(std::is_trivially_default_constructible_v<RotationAxisConfig> ==
              std::is_trivially_default_constructible_v<OriginalRotationAxisConfig>);
IOJ_RECORD_MEMBER_CONTRACT(RotationAxisConfig, manual_semantic);
IOJ_RECORD_MEMBER_CONTRACT(RotationAxisConfig, maximum_rate);
IOJ_RECORD_MEMBER_CONTRACT(RotationAxisConfig, acceleration);
IOJ_RECORD_MEMBER_CONTRACT(RotationAxisConfig, response);
IOJ_RECORD_MEMBER_CONTRACT(RotationAxisConfig, stabilization);
TEST(SimulationModelDefaults, RotationAxisConfig) {
    RotationAxisConfig const value{};
    EXPECT_EQ(value.manual_semantic, (RotationSemantic{RotationSemantic::Disabled}));
    EXPECT_EQ(value.maximum_rate, (float{}));
    EXPECT_EQ(value.acceleration, (float{}));
}
}

namespace ioj::sim::player::model_contract {
struct OriginalRotationAxesConfig {
    RotationAxisConfig pitch{};
    RotationAxisConfig yaw{};
    RotationAxisConfig roll{};
};
static_assert(sizeof(RotationAxesConfig) == sizeof(OriginalRotationAxesConfig));
static_assert(alignof(RotationAxesConfig) == alignof(OriginalRotationAxesConfig));
static_assert(std::is_standard_layout_v<RotationAxesConfig> ==
              std::is_standard_layout_v<OriginalRotationAxesConfig>);
static_assert(std::is_trivially_copyable_v<RotationAxesConfig> ==
              std::is_trivially_copyable_v<OriginalRotationAxesConfig>);
static_assert(std::is_aggregate_v<RotationAxesConfig>);
static_assert(std::is_trivially_default_constructible_v<RotationAxesConfig> ==
              std::is_trivially_default_constructible_v<OriginalRotationAxesConfig>);
IOJ_RECORD_MEMBER_CONTRACT(RotationAxesConfig, pitch);
IOJ_RECORD_MEMBER_CONTRACT(RotationAxesConfig, yaw);
IOJ_RECORD_MEMBER_CONTRACT(RotationAxesConfig, roll);
}

namespace ioj::sim::player::model_contract {
struct OriginalFacingVelocityConfig {
    FacingVelocityCoupling mode{FacingVelocityCoupling::Independent};
    float alignment_rate{};
    ResponseConfig response{};
};
static_assert(sizeof(FacingVelocityConfig) == sizeof(OriginalFacingVelocityConfig));
static_assert(alignof(FacingVelocityConfig) == alignof(OriginalFacingVelocityConfig));
static_assert(std::is_standard_layout_v<FacingVelocityConfig> ==
              std::is_standard_layout_v<OriginalFacingVelocityConfig>);
static_assert(std::is_trivially_copyable_v<FacingVelocityConfig> ==
              std::is_trivially_copyable_v<OriginalFacingVelocityConfig>);
static_assert(std::is_aggregate_v<FacingVelocityConfig>);
static_assert(std::is_trivially_default_constructible_v<FacingVelocityConfig> ==
              std::is_trivially_default_constructible_v<OriginalFacingVelocityConfig>);
IOJ_RECORD_MEMBER_CONTRACT(FacingVelocityConfig, mode);
IOJ_RECORD_MEMBER_CONTRACT(FacingVelocityConfig, alignment_rate);
IOJ_RECORD_MEMBER_CONTRACT(FacingVelocityConfig, response);
TEST(SimulationModelDefaults, FacingVelocityConfig) {
    FacingVelocityConfig const value{};
    EXPECT_EQ(value.mode, (FacingVelocityCoupling{FacingVelocityCoupling::Independent}));
    EXPECT_EQ(value.alignment_rate, (float{}));
}
}

namespace ioj::sim::player::model_contract {
struct OriginalBoostConfig {
    bool available{};
    bool accelerator_activates_boost{};
    float energy_drain_per_second{};
    ResponseConfig response{};
};
static_assert(sizeof(BoostConfig) == sizeof(OriginalBoostConfig));
static_assert(alignof(BoostConfig) == alignof(OriginalBoostConfig));
static_assert(std::is_standard_layout_v<BoostConfig> ==
              std::is_standard_layout_v<OriginalBoostConfig>);
static_assert(std::is_trivially_copyable_v<BoostConfig> ==
              std::is_trivially_copyable_v<OriginalBoostConfig>);
static_assert(std::is_aggregate_v<BoostConfig>);
static_assert(std::is_trivially_default_constructible_v<BoostConfig> ==
              std::is_trivially_default_constructible_v<OriginalBoostConfig>);
IOJ_RECORD_MEMBER_CONTRACT(BoostConfig, available);
IOJ_RECORD_MEMBER_CONTRACT(BoostConfig, accelerator_activates_boost);
IOJ_RECORD_MEMBER_CONTRACT(BoostConfig, energy_drain_per_second);
IOJ_RECORD_MEMBER_CONTRACT(BoostConfig, response);
TEST(SimulationModelDefaults, BoostConfig) {
    BoostConfig const value{};
    EXPECT_EQ(value.available, (bool{}));
    EXPECT_EQ(value.accelerator_activates_boost, (bool{}));
    EXPECT_EQ(value.energy_drain_per_second, (float{}));
}
}

namespace ioj::sim::player::model_contract {
struct OriginalBrakeConfig {
    bool available{};
    float target_speed{};
    float deceleration{};
    float energy_drain_per_second{};
    ResponseConfig response{};
};
static_assert(sizeof(BrakeConfig) == sizeof(OriginalBrakeConfig));
static_assert(alignof(BrakeConfig) == alignof(OriginalBrakeConfig));
static_assert(std::is_standard_layout_v<BrakeConfig> ==
              std::is_standard_layout_v<OriginalBrakeConfig>);
static_assert(std::is_trivially_copyable_v<BrakeConfig> ==
              std::is_trivially_copyable_v<OriginalBrakeConfig>);
static_assert(std::is_aggregate_v<BrakeConfig>);
static_assert(std::is_trivially_default_constructible_v<BrakeConfig> ==
              std::is_trivially_default_constructible_v<OriginalBrakeConfig>);
IOJ_RECORD_MEMBER_CONTRACT(BrakeConfig, available);
IOJ_RECORD_MEMBER_CONTRACT(BrakeConfig, target_speed);
IOJ_RECORD_MEMBER_CONTRACT(BrakeConfig, deceleration);
IOJ_RECORD_MEMBER_CONTRACT(BrakeConfig, energy_drain_per_second);
IOJ_RECORD_MEMBER_CONTRACT(BrakeConfig, response);
TEST(SimulationModelDefaults, BrakeConfig) {
    BrakeConfig const value{};
    EXPECT_EQ(value.available, (bool{}));
    EXPECT_EQ(value.target_speed, (float{}));
    EXPECT_EQ(value.deceleration, (float{}));
    EXPECT_EQ(value.energy_drain_per_second, (float{}));
}
}

namespace ioj::sim::player::model_contract {
struct OriginalFlightModelConfig {
    TranslationAxesConfig translation{};
    RotationAxesConfig rotation{};
    FacingVelocityConfig facing_velocity{};
    BoostConfig boost{};
    BrakeConfig brake{};
    BrakeConfig emergency_brake{};
    float energy_recharge_per_second{};
    float maximum_resultant_speed{effectively_unlimited_speed};
    float boosted_maximum_resultant_speed{effectively_unlimited_speed};
};
static_assert(sizeof(FlightModelConfig) == sizeof(OriginalFlightModelConfig));
static_assert(alignof(FlightModelConfig) == alignof(OriginalFlightModelConfig));
static_assert(std::is_standard_layout_v<FlightModelConfig> ==
              std::is_standard_layout_v<OriginalFlightModelConfig>);
static_assert(std::is_trivially_copyable_v<FlightModelConfig> ==
              std::is_trivially_copyable_v<OriginalFlightModelConfig>);
static_assert(std::is_aggregate_v<FlightModelConfig>);
static_assert(std::is_trivially_default_constructible_v<FlightModelConfig> ==
              std::is_trivially_default_constructible_v<OriginalFlightModelConfig>);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelConfig, translation);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelConfig, rotation);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelConfig, facing_velocity);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelConfig, boost);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelConfig, brake);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelConfig, emergency_brake);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelConfig, energy_recharge_per_second);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelConfig, maximum_resultant_speed);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelConfig, boosted_maximum_resultant_speed);
TEST(SimulationModelDefaults, FlightModelConfig) {
    FlightModelConfig const value{};
    EXPECT_EQ(value.energy_recharge_per_second, (float{}));
    EXPECT_EQ(value.maximum_resultant_speed, (float{effectively_unlimited_speed}));
    EXPECT_EQ(value.boosted_maximum_resultant_speed, (float{effectively_unlimited_speed}));
}
}

namespace ioj::sim::player::model_contract {
struct OriginalFlightModelProfile {
    FlightModelPreset base_preset{FlightModelPreset::Starfox};
    bool customized{};
    FlightModelConfig config{};
};
static_assert(sizeof(FlightModelProfile) == sizeof(OriginalFlightModelProfile));
static_assert(alignof(FlightModelProfile) == alignof(OriginalFlightModelProfile));
static_assert(std::is_standard_layout_v<FlightModelProfile> ==
              std::is_standard_layout_v<OriginalFlightModelProfile>);
static_assert(std::is_trivially_copyable_v<FlightModelProfile> ==
              std::is_trivially_copyable_v<OriginalFlightModelProfile>);
static_assert(std::is_aggregate_v<FlightModelProfile>);
static_assert(std::is_trivially_default_constructible_v<FlightModelProfile> ==
              std::is_trivially_default_constructible_v<OriginalFlightModelProfile>);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelProfile, base_preset);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelProfile, customized);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelProfile, config);
TEST(SimulationModelDefaults, FlightModelProfile) {
    FlightModelProfile const value{};
    EXPECT_EQ(value.base_preset, (FlightModelPreset{FlightModelPreset::Starfox}));
    EXPECT_EQ(value.customized, (bool{}));
}
}

namespace ioj::sim::player::model_contract {
struct OriginalFlightModelLoadout {
    FlightModelProfile up{};
    FlightModelProfile right{};
    FlightModelProfile down{};
    FlightModelProfile left{};
    FlightModelSlot initial_slot{FlightModelSlot::Up};
};
static_assert(sizeof(FlightModelLoadout) == sizeof(OriginalFlightModelLoadout));
static_assert(alignof(FlightModelLoadout) == alignof(OriginalFlightModelLoadout));
static_assert(std::is_standard_layout_v<FlightModelLoadout> ==
              std::is_standard_layout_v<OriginalFlightModelLoadout>);
static_assert(std::is_trivially_copyable_v<FlightModelLoadout> ==
              std::is_trivially_copyable_v<OriginalFlightModelLoadout>);
static_assert(std::is_aggregate_v<FlightModelLoadout>);
static_assert(std::is_trivially_default_constructible_v<FlightModelLoadout> ==
              std::is_trivially_default_constructible_v<OriginalFlightModelLoadout>);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelLoadout, up);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelLoadout, right);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelLoadout, down);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelLoadout, left);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelLoadout, initial_slot);
TEST(SimulationModelDefaults, FlightModelLoadout) {
    FlightModelLoadout const value{};
    EXPECT_EQ(value.initial_slot, (FlightModelSlot{FlightModelSlot::Up}));
}
}

namespace ioj::sim::player::model_contract {
struct OriginalPhysicalMovementState {
    Transform3d transform{};
    ml::Vector3d velocity{};
};
static_assert(sizeof(PhysicalMovementState) == sizeof(OriginalPhysicalMovementState));
static_assert(alignof(PhysicalMovementState) == alignof(OriginalPhysicalMovementState));
static_assert(std::is_standard_layout_v<PhysicalMovementState> ==
              std::is_standard_layout_v<OriginalPhysicalMovementState>);
static_assert(std::is_trivially_copyable_v<PhysicalMovementState> ==
              std::is_trivially_copyable_v<OriginalPhysicalMovementState>);
static_assert(std::is_aggregate_v<PhysicalMovementState>);
static_assert(std::is_trivially_default_constructible_v<PhysicalMovementState> ==
              std::is_trivially_default_constructible_v<OriginalPhysicalMovementState>);
IOJ_RECORD_MEMBER_CONTRACT(PhysicalMovementState, transform);
IOJ_RECORD_MEMBER_CONTRACT(PhysicalMovementState, velocity);
}

namespace ioj::sim::player::model_contract {
struct OriginalPlayerFlightIntent {
    ml::Vector3d translation{};
    ml::Vector3d rotation{};
    float accelerator{};
    bool boost_held{};
    bool brake_held{};
    bool emergency_brake_held{};
};
static_assert(sizeof(PlayerFlightIntent) == sizeof(OriginalPlayerFlightIntent));
static_assert(alignof(PlayerFlightIntent) == alignof(OriginalPlayerFlightIntent));
static_assert(std::is_standard_layout_v<PlayerFlightIntent> ==
              std::is_standard_layout_v<OriginalPlayerFlightIntent>);
static_assert(std::is_trivially_copyable_v<PlayerFlightIntent> ==
              std::is_trivially_copyable_v<OriginalPlayerFlightIntent>);
static_assert(std::is_aggregate_v<PlayerFlightIntent>);
static_assert(std::is_trivially_default_constructible_v<PlayerFlightIntent> ==
              std::is_trivially_default_constructible_v<OriginalPlayerFlightIntent>);
IOJ_RECORD_MEMBER_CONTRACT(PlayerFlightIntent, translation);
IOJ_RECORD_MEMBER_CONTRACT(PlayerFlightIntent, rotation);
IOJ_RECORD_MEMBER_CONTRACT(PlayerFlightIntent, accelerator);
IOJ_RECORD_MEMBER_CONTRACT(PlayerFlightIntent, boost_held);
IOJ_RECORD_MEMBER_CONTRACT(PlayerFlightIntent, brake_held);
IOJ_RECORD_MEMBER_CONTRACT(PlayerFlightIntent, emergency_brake_held);
TEST(SimulationModelDefaults, PlayerFlightIntent) {
    PlayerFlightIntent const value{};
    EXPECT_EQ(value.accelerator, (float{}));
    EXPECT_EQ(value.boost_held, (bool{}));
    EXPECT_EQ(value.brake_held, (bool{}));
    EXPECT_EQ(value.emergency_brake_held, (bool{}));
}
}

namespace ioj::sim::player::model_contract {
struct OriginalFlightModelControllerState {
    ScalarResponse forward_manual_response{};
    ScalarResponse forward_automatic_response{};
    ScalarResponse right_manual_response{};
    ScalarResponse right_automatic_response{};
    ScalarResponse up_manual_response{};
    ScalarResponse up_automatic_response{};
    ScalarResponse pitch_response{};
    ScalarResponse yaw_response{};
    ScalarResponse roll_response{};
    ScalarResponse pitch_stabilization_response{};
    ScalarResponse yaw_stabilization_response{};
    ScalarResponse roll_stabilization_response{};
    ScalarResponse facing_alignment_response{};
    ScalarResponse brake_engagement_response{};
    ml::Vector3d angular_velocity{};
    float persistent_forward_target_speed{};
    float persistent_right_target_speed{};
    float persistent_up_target_speed{};
    ml::Vector3d time_since_rotation_input{100.0, 100.0, 100.0};
    BoostBrakeState effective_action{};
};
static_assert(sizeof(FlightModelControllerState) == sizeof(OriginalFlightModelControllerState));
static_assert(alignof(FlightModelControllerState) == alignof(OriginalFlightModelControllerState));
static_assert(std::is_standard_layout_v<FlightModelControllerState> ==
              std::is_standard_layout_v<OriginalFlightModelControllerState>);
static_assert(std::is_trivially_copyable_v<FlightModelControllerState> ==
              std::is_trivially_copyable_v<OriginalFlightModelControllerState>);
static_assert(std::is_aggregate_v<FlightModelControllerState>);
static_assert(std::is_trivially_default_constructible_v<FlightModelControllerState> ==
              std::is_trivially_default_constructible_v<OriginalFlightModelControllerState>);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelControllerState, forward_manual_response);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelControllerState, forward_automatic_response);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelControllerState, right_manual_response);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelControllerState, right_automatic_response);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelControllerState, up_manual_response);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelControllerState, up_automatic_response);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelControllerState, pitch_response);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelControllerState, yaw_response);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelControllerState, roll_response);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelControllerState, pitch_stabilization_response);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelControllerState, yaw_stabilization_response);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelControllerState, roll_stabilization_response);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelControllerState, facing_alignment_response);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelControllerState, brake_engagement_response);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelControllerState, angular_velocity);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelControllerState, persistent_forward_target_speed);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelControllerState, persistent_right_target_speed);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelControllerState, persistent_up_target_speed);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelControllerState, time_since_rotation_input);
IOJ_RECORD_MEMBER_CONTRACT(FlightModelControllerState, effective_action);
TEST(SimulationModelDefaults, FlightModelControllerState) {
    FlightModelControllerState const value{};
    EXPECT_EQ(value.persistent_forward_target_speed, (float{}));
    EXPECT_EQ(value.persistent_right_target_speed, (float{}));
    EXPECT_EQ(value.persistent_up_target_speed, (float{}));
}
}

namespace ioj::sim::player::model_contract {
struct OriginalPlayerResourceState {
    float thrust_energy{1.f};
    float thrust_change_rate{};
};
static_assert(sizeof(PlayerResourceState) == sizeof(OriginalPlayerResourceState));
static_assert(alignof(PlayerResourceState) == alignof(OriginalPlayerResourceState));
static_assert(std::is_standard_layout_v<PlayerResourceState> ==
              std::is_standard_layout_v<OriginalPlayerResourceState>);
static_assert(std::is_trivially_copyable_v<PlayerResourceState> ==
              std::is_trivially_copyable_v<OriginalPlayerResourceState>);
static_assert(std::is_aggregate_v<PlayerResourceState>);
static_assert(std::is_trivially_default_constructible_v<PlayerResourceState> ==
              std::is_trivially_default_constructible_v<OriginalPlayerResourceState>);
IOJ_RECORD_MEMBER_CONTRACT(PlayerResourceState, thrust_energy);
IOJ_RECORD_MEMBER_CONTRACT(PlayerResourceState, thrust_change_rate);
TEST(SimulationModelDefaults, PlayerResourceState) {
    PlayerResourceState const value{};
    EXPECT_EQ(value.thrust_energy, (float{1.f}));
    EXPECT_EQ(value.thrust_change_rate, (float{}));
}
}

namespace ioj::sim::player::model_contract {
struct OriginalPlayerPresentationState {
    Transform3d body_transform{};
    std::uint64_t boost_start_sequence{};
};
static_assert(sizeof(PlayerPresentationState) == sizeof(OriginalPlayerPresentationState));
static_assert(alignof(PlayerPresentationState) == alignof(OriginalPlayerPresentationState));
static_assert(std::is_standard_layout_v<PlayerPresentationState> ==
              std::is_standard_layout_v<OriginalPlayerPresentationState>);
static_assert(std::is_trivially_copyable_v<PlayerPresentationState> ==
              std::is_trivially_copyable_v<OriginalPlayerPresentationState>);
static_assert(std::is_aggregate_v<PlayerPresentationState>);
static_assert(std::is_trivially_default_constructible_v<PlayerPresentationState> ==
              std::is_trivially_default_constructible_v<OriginalPlayerPresentationState>);
IOJ_RECORD_MEMBER_CONTRACT(PlayerPresentationState, body_transform);
IOJ_RECORD_MEMBER_CONTRACT(PlayerPresentationState, boost_start_sequence);
TEST(SimulationModelDefaults, PlayerPresentationState) {
    PlayerPresentationState const value{};
    EXPECT_EQ(value.boost_start_sequence, (std::uint64_t{}));
}
}

namespace ioj::sim::player::model_contract {
struct OriginalPlayerSimulationState {
    PhysicalMovementState physical{};
    FlightModelControllerState controller{};
    PlayerResourceState resources{};
    PlayerPresentationState presentation{};
};
static_assert(sizeof(PlayerSimulationState) == sizeof(OriginalPlayerSimulationState));
static_assert(alignof(PlayerSimulationState) == alignof(OriginalPlayerSimulationState));
static_assert(std::is_standard_layout_v<PlayerSimulationState> ==
              std::is_standard_layout_v<OriginalPlayerSimulationState>);
static_assert(std::is_trivially_copyable_v<PlayerSimulationState> ==
              std::is_trivially_copyable_v<OriginalPlayerSimulationState>);
static_assert(std::is_aggregate_v<PlayerSimulationState>);
static_assert(std::is_trivially_default_constructible_v<PlayerSimulationState> ==
              std::is_trivially_default_constructible_v<OriginalPlayerSimulationState>);
IOJ_RECORD_MEMBER_CONTRACT(PlayerSimulationState, physical);
IOJ_RECORD_MEMBER_CONTRACT(PlayerSimulationState, controller);
IOJ_RECORD_MEMBER_CONTRACT(PlayerSimulationState, resources);
IOJ_RECORD_MEMBER_CONTRACT(PlayerSimulationState, presentation);
}

namespace ioj::sim::model_contract {
struct OriginalLaserSource {
    Team team{Team::White};
    EntityType type{EntityType::TubeSpinner};
};
static_assert(sizeof(LaserSource) == sizeof(OriginalLaserSource));
static_assert(alignof(LaserSource) == alignof(OriginalLaserSource));
static_assert(std::is_standard_layout_v<LaserSource> ==
              std::is_standard_layout_v<OriginalLaserSource>);
static_assert(std::is_trivially_copyable_v<LaserSource> ==
              std::is_trivially_copyable_v<OriginalLaserSource>);
static_assert(std::is_aggregate_v<LaserSource>);
static_assert(std::is_trivially_default_constructible_v<LaserSource> ==
              std::is_trivially_default_constructible_v<OriginalLaserSource>);
IOJ_RECORD_MEMBER_CONTRACT(LaserSource, team);
IOJ_RECORD_MEMBER_CONTRACT(LaserSource, type);
TEST(SimulationModelDefaults, LaserSource) {
    LaserSource const value{};
    EXPECT_EQ(value.team, (Team{Team::White}));
    EXPECT_EQ(value.type, (EntityType{EntityType::TubeSpinner}));
}
}

namespace ioj::sim::model_contract {
struct OriginalIndexSpan {
    std::int32_t offset{0};
    std::int32_t count{0};
};
static_assert(sizeof(IndexSpan) == sizeof(OriginalIndexSpan));
static_assert(alignof(IndexSpan) == alignof(OriginalIndexSpan));
static_assert(std::is_standard_layout_v<IndexSpan> == std::is_standard_layout_v<OriginalIndexSpan>);
static_assert(std::is_trivially_copyable_v<IndexSpan> ==
              std::is_trivially_copyable_v<OriginalIndexSpan>);
static_assert(std::is_aggregate_v<IndexSpan>);
static_assert(std::is_trivially_default_constructible_v<IndexSpan> ==
              std::is_trivially_default_constructible_v<OriginalIndexSpan>);
IOJ_RECORD_MEMBER_CONTRACT(IndexSpan, offset);
IOJ_RECORD_MEMBER_CONTRACT(IndexSpan, count);
TEST(SimulationModelDefaults, IndexSpan) {
    IndexSpan const value{};
    EXPECT_EQ(value.offset, (std::int32_t{0}));
    EXPECT_EQ(value.count, (std::int32_t{0}));
}
}

namespace ioj::sim::model_contract {
struct OriginalHealthMove {
    EntityUniqueId owner{};
    HealthIndex old_index{};
    HealthIndex new_index{};
};
static_assert(sizeof(HealthMove) == sizeof(OriginalHealthMove));
static_assert(alignof(HealthMove) == alignof(OriginalHealthMove));
static_assert(std::is_standard_layout_v<HealthMove> ==
              std::is_standard_layout_v<OriginalHealthMove>);
static_assert(std::is_trivially_copyable_v<HealthMove> ==
              std::is_trivially_copyable_v<OriginalHealthMove>);
static_assert(std::is_aggregate_v<HealthMove>);
static_assert(std::is_trivially_default_constructible_v<HealthMove> ==
              std::is_trivially_default_constructible_v<OriginalHealthMove>);
IOJ_RECORD_MEMBER_CONTRACT(HealthMove, owner);
IOJ_RECORD_MEMBER_CONTRACT(HealthMove, old_index);
IOJ_RECORD_MEMBER_CONTRACT(HealthMove, new_index);
}

namespace ioj::sim::model_contract {
struct OriginalLineTraceResult {
    Vector3f location{};
    EntityUniqueId entity;
    collision::StaticGeometryIndex static_geometry_index{collision::invalid_static_geometry_index};
    bool hit{false};
};
static_assert(sizeof(LineTraceResult) == sizeof(OriginalLineTraceResult));
static_assert(alignof(LineTraceResult) == alignof(OriginalLineTraceResult));
static_assert(std::is_standard_layout_v<LineTraceResult> ==
              std::is_standard_layout_v<OriginalLineTraceResult>);
static_assert(std::is_trivially_copyable_v<LineTraceResult> ==
              std::is_trivially_copyable_v<OriginalLineTraceResult>);
static_assert(std::is_aggregate_v<LineTraceResult>);
static_assert(std::is_trivially_default_constructible_v<LineTraceResult> ==
              std::is_trivially_default_constructible_v<OriginalLineTraceResult>);
IOJ_RECORD_MEMBER_CONTRACT(LineTraceResult, location);
IOJ_RECORD_MEMBER_CONTRACT(LineTraceResult, entity);
IOJ_RECORD_MEMBER_CONTRACT(LineTraceResult, static_geometry_index);
IOJ_RECORD_MEMBER_CONTRACT(LineTraceResult, hit);
TEST(SimulationModelDefaults, LineTraceResult) {
    LineTraceResult const value{};
    EXPECT_EQ(value.static_geometry_index,
              (collision::StaticGeometryIndex{collision::invalid_static_geometry_index}));
    EXPECT_EQ(value.hit, (bool{false}));
}
}

namespace ioj::sim::model_contract {
struct OriginalCapitalDeathEvent {
    Vector3f location;
    std::int32_t batch_index{};
};
static_assert(sizeof(CapitalDeathEvent) == sizeof(OriginalCapitalDeathEvent));
static_assert(alignof(CapitalDeathEvent) == alignof(OriginalCapitalDeathEvent));
static_assert(std::is_standard_layout_v<CapitalDeathEvent> ==
              std::is_standard_layout_v<OriginalCapitalDeathEvent>);
static_assert(std::is_trivially_copyable_v<CapitalDeathEvent> ==
              std::is_trivially_copyable_v<OriginalCapitalDeathEvent>);
static_assert(std::is_aggregate_v<CapitalDeathEvent>);
static_assert(std::is_trivially_default_constructible_v<CapitalDeathEvent> ==
              std::is_trivially_default_constructible_v<OriginalCapitalDeathEvent>);
IOJ_RECORD_MEMBER_CONTRACT(CapitalDeathEvent, location);
IOJ_RECORD_MEMBER_CONTRACT(CapitalDeathEvent, batch_index);
TEST(SimulationModelDefaults, CapitalDeathEvent) {
    CapitalDeathEvent const value{};
    EXPECT_EQ(value.batch_index, (std::int32_t{}));
}
}

namespace ioj::sim::fighters::model_contract {
struct OriginalNavigationTelemetrySnapshot {
    std::int32_t separating_fighter_count{};
    std::int32_t avoiding_fighter_count{};
    std::int32_t clear_risk_count{};
    std::int32_t nearby_risk_count{};
    std::int32_t active_risk_count{};
    std::int32_t immediate_risk_count{};
    std::int32_t separation_query_count{};
    std::int32_t separation_candidate_count{};
    std::int32_t dense_direction_selection_count{};
    std::int32_t steering_memory_fighter_count{};
    std::int32_t hard_trace_count{};
};
static_assert(sizeof(NavigationTelemetrySnapshot) == sizeof(OriginalNavigationTelemetrySnapshot));
static_assert(alignof(NavigationTelemetrySnapshot) == alignof(OriginalNavigationTelemetrySnapshot));
static_assert(std::is_standard_layout_v<NavigationTelemetrySnapshot> ==
              std::is_standard_layout_v<OriginalNavigationTelemetrySnapshot>);
static_assert(std::is_trivially_copyable_v<NavigationTelemetrySnapshot> ==
              std::is_trivially_copyable_v<OriginalNavigationTelemetrySnapshot>);
static_assert(std::is_aggregate_v<NavigationTelemetrySnapshot>);
static_assert(std::is_trivially_default_constructible_v<NavigationTelemetrySnapshot> ==
              std::is_trivially_default_constructible_v<OriginalNavigationTelemetrySnapshot>);
IOJ_RECORD_MEMBER_CONTRACT(NavigationTelemetrySnapshot, separating_fighter_count);
IOJ_RECORD_MEMBER_CONTRACT(NavigationTelemetrySnapshot, avoiding_fighter_count);
IOJ_RECORD_MEMBER_CONTRACT(NavigationTelemetrySnapshot, clear_risk_count);
IOJ_RECORD_MEMBER_CONTRACT(NavigationTelemetrySnapshot, nearby_risk_count);
IOJ_RECORD_MEMBER_CONTRACT(NavigationTelemetrySnapshot, active_risk_count);
IOJ_RECORD_MEMBER_CONTRACT(NavigationTelemetrySnapshot, immediate_risk_count);
IOJ_RECORD_MEMBER_CONTRACT(NavigationTelemetrySnapshot, separation_query_count);
IOJ_RECORD_MEMBER_CONTRACT(NavigationTelemetrySnapshot, separation_candidate_count);
IOJ_RECORD_MEMBER_CONTRACT(NavigationTelemetrySnapshot, dense_direction_selection_count);
IOJ_RECORD_MEMBER_CONTRACT(NavigationTelemetrySnapshot, steering_memory_fighter_count);
IOJ_RECORD_MEMBER_CONTRACT(NavigationTelemetrySnapshot, hard_trace_count);
TEST(SimulationModelDefaults, NavigationTelemetrySnapshot) {
    NavigationTelemetrySnapshot const value{};
    EXPECT_EQ(value.separating_fighter_count, (std::int32_t{}));
    EXPECT_EQ(value.avoiding_fighter_count, (std::int32_t{}));
    EXPECT_EQ(value.clear_risk_count, (std::int32_t{}));
    EXPECT_EQ(value.nearby_risk_count, (std::int32_t{}));
    EXPECT_EQ(value.active_risk_count, (std::int32_t{}));
    EXPECT_EQ(value.immediate_risk_count, (std::int32_t{}));
    EXPECT_EQ(value.separation_query_count, (std::int32_t{}));
    EXPECT_EQ(value.separation_candidate_count, (std::int32_t{}));
    EXPECT_EQ(value.dense_direction_selection_count, (std::int32_t{}));
    EXPECT_EQ(value.steering_memory_fighter_count, (std::int32_t{}));
    EXPECT_EQ(value.hard_trace_count, (std::int32_t{}));
}
}

namespace ioj::sim::model_contract {
struct OriginalLevelTelemetryCurrentState {
    telemetry::EntityCounts active_entities_by_team_and_type{};
    std::int32_t active_entities{};
    std::int32_t spawned_entities{};
    std::int32_t destroyed_entities{};
    std::int32_t kills{};
    std::int32_t active_lasers{};
    std::int32_t lasers_fired{};
};
static_assert(sizeof(LevelTelemetryCurrentState) == sizeof(OriginalLevelTelemetryCurrentState));
static_assert(alignof(LevelTelemetryCurrentState) == alignof(OriginalLevelTelemetryCurrentState));
static_assert(std::is_standard_layout_v<LevelTelemetryCurrentState> ==
              std::is_standard_layout_v<OriginalLevelTelemetryCurrentState>);
static_assert(std::is_trivially_copyable_v<LevelTelemetryCurrentState> ==
              std::is_trivially_copyable_v<OriginalLevelTelemetryCurrentState>);
static_assert(std::is_aggregate_v<LevelTelemetryCurrentState>);
static_assert(std::is_trivially_default_constructible_v<LevelTelemetryCurrentState> ==
              std::is_trivially_default_constructible_v<OriginalLevelTelemetryCurrentState>);
IOJ_RECORD_MEMBER_CONTRACT(LevelTelemetryCurrentState, active_entities_by_team_and_type);
IOJ_RECORD_MEMBER_CONTRACT(LevelTelemetryCurrentState, active_entities);
IOJ_RECORD_MEMBER_CONTRACT(LevelTelemetryCurrentState, spawned_entities);
IOJ_RECORD_MEMBER_CONTRACT(LevelTelemetryCurrentState, destroyed_entities);
IOJ_RECORD_MEMBER_CONTRACT(LevelTelemetryCurrentState, kills);
IOJ_RECORD_MEMBER_CONTRACT(LevelTelemetryCurrentState, active_lasers);
IOJ_RECORD_MEMBER_CONTRACT(LevelTelemetryCurrentState, lasers_fired);
TEST(SimulationModelDefaults, LevelTelemetryCurrentState) {
    LevelTelemetryCurrentState const value{};
    EXPECT_EQ(value.active_entities, (std::int32_t{}));
    EXPECT_EQ(value.spawned_entities, (std::int32_t{}));
    EXPECT_EQ(value.destroyed_entities, (std::int32_t{}));
    EXPECT_EQ(value.kills, (std::int32_t{}));
    EXPECT_EQ(value.active_lasers, (std::int32_t{}));
    EXPECT_EQ(value.lasers_fired, (std::int32_t{}));
}
}

namespace ioj::sim::model_contract {
static_assert(std::is_same_v<std::underlying_type_t<SimulationPhase>, int>);
static_assert(sizeof(HealthIndex) == 4 && alignof(HealthIndex) == alignof(std::uint32_t));
static_assert(!std::is_convertible_v<std::uint32_t, HealthIndex>);
static_assert(!std::is_aggregate_v<HealthIndex>);
static_assert(!HealthIndex{}.is_valid());
static_assert(HealthIndex{}.raw_value() == 0xffffffffU);
static_assert(HealthIndex{4}.raw_value() == 4);
static_assert(std::is_same_v<Vector3f, HMM_Vec3>);
static_assert(std::is_same_v<Quaternion4f, HMM_Quat>);
struct OriginalLineTraces {
    Vectors3f starts;
    Vectors3f ends;
};
struct OriginalTraceHits {
    Vectors3f locations;
    ml::native_soa::Vector<EntityUniqueId> entities;
    ml::native_soa::Vector<collision::StaticGeometryIndex> static_geometry_indices;
    ml::native_soa::Vector<TraceHit> hits;
};
static_assert(sizeof(LineTraces) == sizeof(OriginalLineTraces));
static_assert(alignof(LineTraces) == alignof(OriginalLineTraces));
static_assert(sizeof(TraceHits) == sizeof(OriginalTraceHits));
static_assert(alignof(TraceHits) == alignof(OriginalTraceHits));
IOJ_RECORD_MEMBER_CONTRACT(LineTraces, starts);
IOJ_RECORD_MEMBER_CONTRACT(LineTraces, ends);
IOJ_RECORD_MEMBER_CONTRACT(TraceHits, locations);
IOJ_RECORD_MEMBER_CONTRACT(TraceHits, entities);
IOJ_RECORD_MEMBER_CONTRACT(TraceHits, static_geometry_indices);
IOJ_RECORD_MEMBER_CONTRACT(TraceHits, hits);
static_assert(noexcept(std::declval<LineTraces&>().reset()));
static_assert(noexcept(std::declval<TraceHits&>().reset()));
TEST(SimulationModelDefaults, RotationInputDelay) {
    player::FlightModelControllerState const state{};
    EXPECT_EQ(state.time_since_rotation_input.x, 100.0);
    EXPECT_EQ(state.time_since_rotation_input.y, 100.0);
    EXPECT_EQ(state.time_since_rotation_input.z, 100.0);
}
}
#undef IOJ_RECORD_MEMBER_CONTRACT

namespace ioj::sim::model_contract {
static_assert(std::is_same_v<std::underlying_type_t<OrchestratorState>, std::uint8_t>);
static_assert(static_cast<std::uint8_t>(OrchestratorState::Uninitialised) == 0);
static_assert(static_cast<std::uint8_t>(OrchestratorState::Paused) == 1);
static_assert(static_cast<std::uint8_t>(OrchestratorState::Running) == 2);
static_assert(static_cast<std::uint8_t>(OrchestratorState::Stopped) == 3);
}

namespace ioj::sim::model_contract {
static_assert(std::is_same_v<std::underlying_type_t<SimulationPhase>, int>);
static_assert(static_cast<int>(SimulationPhase::Initialisation) == 0);
static_assert(static_cast<int>(SimulationPhase::Preparation) == 1);
static_assert(static_cast<int>(SimulationPhase::Thinking) == 2);
static_assert(static_cast<int>(SimulationPhase::Action) == 3);
static_assert(static_cast<int>(SimulationPhase::Resolution) == 4);
static_assert(static_cast<int>(SimulationPhase::ResolutionCommit) == 5);
static_assert(static_cast<int>(SimulationPhase::Idle) == 6);
}

namespace ioj::sim::collision::model_contract {
static_assert(std::is_same_v<std::underlying_type_t<TraceEntityFilter>, std::uint8_t>);
static_assert(static_cast<std::uint8_t>(TraceEntityFilter::None) == 0);
static_assert(static_cast<std::uint8_t>(TraceEntityFilter::ExcludeFighters) == 1);
}

namespace ioj::sim::levels::model_contract {
static_assert(std::is_same_v<std::underlying_type_t<LevelMissionMode>, std::uint8_t>);
static_assert(static_cast<std::uint8_t>(LevelMissionMode::Unspecified) == 0);
static_assert(static_cast<std::uint8_t>(LevelMissionMode::SurviveTime) == 1);
static_assert(static_cast<std::uint8_t>(LevelMissionMode::KillEnemies) == 2);
static_assert(static_cast<std::uint8_t>(LevelMissionMode::KillEnemiesWithinTime) == 3);
}

namespace ioj::sim::levels::model_contract {
static_assert(std::is_same_v<std::underlying_type_t<LevelValidationErrorCode>, std::uint8_t>);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::MissingLevelId) == 0);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::MissingTitle) == 1);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::InvalidParTime) == 2);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::UnexpectedParTime) == 3);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::MissingViewpoint) == 4);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::ConflictingViewpoints) == 5);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::PlayerEntityNotFound) == 6);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::MismatchedEntityColumns) == 7);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::EmptyTeamId) == 8);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::DuplicateTeamId) == 9);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::UnsupportedTeamId) == 10);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::UnknownTeamReference) == 11);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::EmptyArchetypeId) == 12);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::UnsupportedArchetype) == 13);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::ArchetypeRoleMismatch) == 14);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::DuplicateEntityId) == 15);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::InvalidPlacement) == 16);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::InvalidSpawnTime) == 17);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::DelayedPlayerSpawn) == 18);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::MissingCameraTarget) == 19);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::DuplicateCameraTarget) == 20);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::CameraTargetNotFound) == 21);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::InvalidCameraDistance) == 22);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::InvalidCameraOffsetDirection) ==
              23);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::InvalidLevelSize) == 24);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::InvalidGridCellSize) == 25);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::InvalidGridDimensions) == 26);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::MissingMissionMode) == 27);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::UnsupportedMissionMode) == 28);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::InvalidMissionTimeLimit) == 29);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::UnexpectedMissionTimeLimit) ==
              30);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::InvalidMissionKillCount) == 31);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::UnexpectedMissionKillCount) ==
              32);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::MissingMissionHeroes) == 33);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::MissingMissionSurvivors) == 34);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::MissionEntityNotFound) == 35);
static_assert(
    static_cast<std::uint8_t>(LevelValidationErrorCode::DuplicateMissionEntityReference) == 36);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::ConflictingMissionEntityRoles) ==
              37);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::AmbiguousAutomaticKillTeams) ==
              38);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::MissingUnlockLevelId) == 39);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::DuplicateUnlockCriterion) == 40);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::SelfUnlockDependency) == 41);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::UnexpectedMissionEvent) == 42);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::InvalidMissionEventTime) == 43);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::InvalidMissionKillIncrease) ==
              44);
static_assert(static_cast<std::uint8_t>(LevelValidationErrorCode::MissionEventBeforeEntitySpawn) ==
              45);
}

namespace ioj::sim::player::model_contract {
static_assert(std::is_same_v<std::underlying_type_t<FlightModelPreset>, std::uint8_t>);
static_assert(static_cast<std::uint8_t>(FlightModelPreset::Starfox) == 0);
static_assert(static_cast<std::uint8_t>(FlightModelPreset::Fighter) == 1);
static_assert(static_cast<std::uint8_t>(FlightModelPreset::Skater) == 2);
static_assert(static_cast<std::uint8_t>(FlightModelPreset::Gunship) == 3);
static_assert(std::is_same_v<std::underlying_type_t<FlightModelSlot>, std::uint8_t>);
static_assert(static_cast<std::uint8_t>(FlightModelSlot::Up) == 0);
static_assert(static_cast<std::uint8_t>(FlightModelSlot::Right) == 1);
static_assert(static_cast<std::uint8_t>(FlightModelSlot::Down) == 2);
static_assert(static_cast<std::uint8_t>(FlightModelSlot::Left) == 3);
static_assert(std::is_same_v<std::underlying_type_t<TranslationSemantic>, std::uint8_t>);
static_assert(static_cast<std::uint8_t>(TranslationSemantic::Disabled) == 0);
static_assert(static_cast<std::uint8_t>(TranslationSemantic::TargetSpeed) == 1);
static_assert(static_cast<std::uint8_t>(TranslationSemantic::TargetVelocity) == 2);
static_assert(static_cast<std::uint8_t>(TranslationSemantic::Acceleration) == 3);
static_assert(std::is_same_v<std::underlying_type_t<RotationSemantic>, std::uint8_t>);
static_assert(static_cast<std::uint8_t>(RotationSemantic::Disabled) == 0);
static_assert(static_cast<std::uint8_t>(RotationSemantic::TargetAngularVelocity) == 1);
static_assert(static_cast<std::uint8_t>(RotationSemantic::AngularAcceleration) == 2);
static_assert(std::is_same_v<std::underlying_type_t<ReferenceFrame>, std::uint8_t>);
static_assert(static_cast<std::uint8_t>(ReferenceFrame::Ship) == 0);
static_assert(static_cast<std::uint8_t>(ReferenceFrame::World) == 1);
static_assert(std::is_same_v<std::underlying_type_t<TranslationInputSource>, std::uint8_t>);
static_assert(static_cast<std::uint8_t>(TranslationInputSource::Axis) == 0);
static_assert(static_cast<std::uint8_t>(TranslationInputSource::Accelerator) == 1);
static_assert(std::is_same_v<std::underlying_type_t<ResponseMode>, std::uint8_t>);
static_assert(static_cast<std::uint8_t>(ResponseMode::Direct) == 0);
static_assert(static_cast<std::uint8_t>(ResponseMode::RateLimited) == 1);
static_assert(static_cast<std::uint8_t>(ResponseMode::SecondOrder) == 2);
static_assert(std::is_same_v<std::underlying_type_t<FacingVelocityCoupling>, std::uint8_t>);
static_assert(static_cast<std::uint8_t>(FacingVelocityCoupling::Independent) == 0);
static_assert(static_cast<std::uint8_t>(FacingVelocityCoupling::AlignToFacing) == 1);
static_assert(static_cast<std::uint8_t>(FacingVelocityCoupling::LockedToFacing) == 2);
static_assert(std::is_same_v<std::underlying_type_t<FlightModelConfigError>, std::uint8_t>);
static_assert(static_cast<std::uint8_t>(FlightModelConfigError::InvalidEnumValue) == 0);
static_assert(static_cast<std::uint8_t>(FlightModelConfigError::NonFiniteValue) == 1);
static_assert(static_cast<std::uint8_t>(FlightModelConfigError::NegativeValue) == 2);
static_assert(static_cast<std::uint8_t>(FlightModelConfigError::InputValueOutOfRange) == 3);
static_assert(static_cast<std::uint8_t>(FlightModelConfigError::InvalidManualChannel) == 4);
static_assert(static_cast<std::uint8_t>(FlightModelConfigError::InvalidAutomaticChannel) == 5);
static_assert(static_cast<std::uint8_t>(FlightModelConfigError::AmbiguousTargetChannels) == 6);
static_assert(static_cast<std::uint8_t>(FlightModelConfigError::InvalidSecondOrderSettlingTime) ==
              7);
static_assert(static_cast<std::uint8_t>(FlightModelConfigError::InvalidSecondOrderDampingRatio) ==
              8);
}
