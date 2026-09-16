#include "test_fighter_attack.h"
#include <ioj/sim/rotator_math.h>
#include "../support/simulation_test_support.h"

#include <ioj/sim/world_aabb_operations.h>

#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/entity_registry.h>
#include <ioj/sim/fighters/sim.h>
#include <ioj/sim/registry_entity_data.h>

namespace ioj::sim {
namespace fighter_navigation_test {
inline constexpr std::int32_t collision_resilient_health{1'000'000};
}

void run_worldless_fighter_obstacle_avoidance(tests::SimulationFixture const& config) {
    constexpr float source_x{-15000.f};
    constexpr float target_x{15000.f};
    Vector3f const obstacle_min{{-1000.f, -1500.f, -1500.f}};
    Vector3f const obstacle_max{{1000.f, 1500.f, 1500.f}};

    auto data{tests::make_simulation_data(config)};
    data.fighters.health = fighter_navigation_test::collision_resilient_health;
    data.fighters.speed = 4000.f;
    data.fighters.avoidance_update_frequency = 5.f;
    data.fighters.avoidance_lookahead_time = 1.f;
    data.fighters.avoidance_clearance_buffer = 100.f;
    data.capital_ships.fighter_spawn_slots = 1;
    data.capital_ships.fighter_spawn_slots_relative_transforms = {
        {.location = {3000.0, 7000.0, 0.0}}};
    data.static_bounds.add_defaulted(1);
    collision::set(data.static_bounds, 0, obstacle_min, obstacle_max);
    tests::add_capital_spawn(
        data, Vector3f{{source_x, -7000.f, 0.f}}, Team::Green, 1, 0.f, 60.f, 100000);
    tests::add_capital_spawn(
        data, Vector3f{{target_x, 0.f, 0.f}}, Team::Red, 0, 60.f, 60.f, 100000);
    auto const fighter_radius{
        collision::get_entity_radius(data.entity_bounds, collision::EntityAABBs::fighter_index)};

    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& fighters{harness.get_simulation().get_fighters()};
    auto const clearance{fighter_radius + 100.f};
    auto const expanded_min{obstacle_min - Vector3f{{clearance, clearance, clearance}}};
    auto const expanded_max{obstacle_max + Vector3f{{clearance, clearance, clearance}}};
    bool fighter_spawned{};
    bool entered_expanded_obstacle{};
    float maximum_lateral_distance{};
    float maximum_x{-std::numeric_limits<float>::infinity()};

    harness.on_end_tick = [&](LevelSim&) {
        auto const locations{fighters.get_locations()};
        if (locations.num() == 0) {
            return;
        }

        fighter_spawned = true;
        auto const location{locations[0]};
        maximum_lateral_distance =
            std::max(maximum_lateral_distance, std::hypot(location.Y, location.Z));
        maximum_x = std::max(maximum_x, location.X);
        entered_expanded_obstacle = entered_expanded_obstacle ||
                                    (location.X >= expanded_min.X && location.X <= expanded_max.X &&
                                     location.Y >= expanded_min.Y && location.Y <= expanded_max.Y &&
                                     location.Z >= expanded_min.Z && location.Z <= expanded_max.Z);
    };
    harness.timeline.finish_at(14.0);
    tests::expect_true(harness.run_until_timeline_finished(15.0),
                       "Fighter obstacle-avoidance timeline completes");

    tests::expect_true(fighter_spawned, "Avoidance fighter spawned");
    tests::expect_true(!entered_expanded_obstacle,
                       "Fighter remains outside obstacle clearance bounds");
    tests::expect_true(maximum_lateral_distance > expanded_max.Y,
                       "Fighter steers visibly around the obstacle");
    tests::expect_true(maximum_x > expanded_max.X, "Fighter progresses past the obstacle");
}

auto make_fighter_navigation_test_data(tests::SimulationFixture const& config,
                                       std::vector<Transform3d> spawn_slots,
                                       Vector3f const source_location,
                                       Vector3f const target_location) -> LevelSimInitData {
    auto data{tests::make_simulation_data(config)};
    data.fighters.health = fighter_navigation_test::collision_resilient_health;
    data.fighters.speed = 4000.f;
    data.fighters.avoidance_lookahead_time = 1.f;
    data.fighters.laser.max_distance = 2000.f;
    data.fighters.attack_engagement_threshold = 1000000.f;
    data.fighters.attack_distance_band.minimum_ratio = 0.f;
    data.fighters.attack_distance_band.desired_ratio = 0.f;
    data.fighters.attack_distance_band.maximum_ratio = 0.f;
    data.fighters.separation_radius = 1500.f;
    data.fighters.separation_strength = 1.5f;
    data.capital_ships.fighter_spawn_slots = static_cast<std::int32_t>(spawn_slots.size());
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    for (auto const& transform : spawn_slots) {
        data.capital_ships.fighter_spawn_slots_relative_transforms.push_back(transform);
    }
    tests::add_capital_spawn(data, source_location, Team::Green, 1, 0.f, 60.f, 100000);
    tests::add_capital_spawn(data, target_location, Team::Red, -1, 60.f, 60.f, 100000);
    return data;
}

void run_worldless_fighter_capital_obstruction(tests::SimulationFixture const& config) {
    Vector3f const source{{-18000.f, -7000.f, 0.f}};
    Vector3f const obstacle{{0.f, 0.f, 0.f}};
    Vector3f const target{{18000.f, 0.f, 0.f}};
    auto data{tests::make_simulation_data(config)};
    data.fighters.health = fighter_navigation_test::collision_resilient_health;
    data.fighters.speed = 4000.f;
    data.fighters.avoidance_lookahead_time = 1.f;
    data.fighters.laser.max_distance = 2000.f;
    data.fighters.attack_engagement_threshold = 1000000.f;
    data.fighters.attack_distance_band.minimum_ratio = 0.f;
    data.fighters.attack_distance_band.desired_ratio = 0.f;
    data.fighters.attack_distance_band.maximum_ratio = 0.f;
    data.capital_ships.fighter_spawn_slots = 1;
    data.capital_ships.fighter_spawn_slots_relative_transforms = {
        {.location = {3000.0, 7000.0, 0.0}}};
    tests::add_capital_spawn(data, source, Team::Green, 2, 0.f, 60.f, 100000);
    tests::add_capital_spawn(data, obstacle, Team::Green, -1, 60.f, 60.f, 100000);
    tests::add_capital_spawn(data, target, Team::Red, -1, 60.f, 60.f, 100000);
    Rotator3f const obstacle_rotation{0.f, 35.f, 0.f};
    data.level_events.initial_spawns.capital_spawns.get_view().view_rotations().set(
        1, obstacle_rotation);

    auto const obstacle_bounds{
        collision::make_entity_world_bounds(data.entity_bounds,
                                            collision::EntityAABBs::capital_ship_index,
                                            obstacle,
                                            to_quaternion(obstacle_rotation))};
    auto const capital_half_extent{((obstacle_bounds.max - obstacle_bounds.min) * 0.5f)};
    auto const fighter_radius{
        collision::get_entity_radius(data.entity_bounds, collision::EntityAABBs::fighter_index)};
    auto const clearance{fighter_radius + data.fighters.avoidance_clearance_buffer};
    Vector3f const clearance_extent{{clearance, clearance, clearance}};
    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& fighters{harness.get_simulation().get_fighters()};
    auto const expanded_min{obstacle_bounds.min - clearance_extent};
    auto const expanded_max{obstacle_bounds.max + clearance_extent};
    bool entered_obstacle{};
    bool saw_avoidance{};
    float maximum_lateral_distance{};
    float maximum_x{-std::numeric_limits<float>::max()};
    harness.on_end_tick = [&](LevelSim&) {
        auto const locations{fighters.get_locations()};
        if (locations.num() == 0) {
            return;
        }
        auto const location{locations[0]};
        entered_obstacle =
            entered_obstacle || (location.X >= expanded_min.X && location.X <= expanded_max.X &&
                                 location.Y >= expanded_min.Y && location.Y <= expanded_max.Y &&
                                 location.Z >= expanded_min.Z && location.Z <= expanded_max.Z);
        maximum_lateral_distance =
            std::max(maximum_lateral_distance, std::hypot(location.Y, location.Z));
        maximum_x = std::max(maximum_x, location.X);
        saw_avoidance =
            saw_avoidance || fighters.get_navigation_telemetry().avoiding_fighter_count > 0;
    };
    harness.timeline.finish_at(14.0);
    tests::expect_true(harness.run_until_timeline_finished(15.0),
                       "Capital-obstruction timeline completes");
    tests::expect_true(!entered_obstacle, "Fighter remains outside capital clearance bounds");
    tests::expect_true(saw_avoidance, "Fighter selects hard avoidance around the capital");
    tests::expect_true(maximum_lateral_distance > capital_half_extent.Y,
                       "Fighter travels laterally around the capital");
    tests::expect_true(maximum_x > expanded_max.X, "Fighter progresses beyond the capital");
}

void run_worldless_fighter_clear_navigation(tests::SimulationFixture const& config) {
    auto data{make_fighter_navigation_test_data(
        config,
        {Transform3d{.location = ml::Vector3d{3000.f, 7000.f, 0.f}}},
        Vector3f{{-20000.f, -7000.f, 0.f}},
        Vector3f{{20000.f, 0.f, 0.f}})};
    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& fighters{harness.get_simulation().get_fighters()};
    Vector3f first_location{Vector3f{}};
    Vector3f last_location{Vector3f{}};
    bool recorded_first{};
    bool recorded_first_movement{};
    bool scanned_before_first_movement{};
    bool saw_separation{};
    bool saw_avoidance{};
    harness.on_end_tick = [&](LevelSim&) {
        auto const locations{fighters.get_locations()};
        if (locations.num() == 0) {
            return;
        }
        last_location = locations[0];
        if (!recorded_first) {
            first_location = last_location;
            recorded_first = true;
            recorded_first_movement = true;
            scanned_before_first_movement =
                fighters.get_navigation_telemetry().hard_trace_count > 0;
        }
        auto const& telemetry{fighters.get_navigation_telemetry()};
        if (!recorded_first_movement && !(std::abs(last_location.X - first_location.X) <= 1.e-4f &&
                                          std::abs(last_location.Y - first_location.Y) <= 1.e-4f &&
                                          std::abs(last_location.Z - first_location.Z) <= 1.e-4f)) {
            recorded_first_movement = true;
            scanned_before_first_movement = telemetry.hard_trace_count > 0;
        }
        saw_separation = saw_separation || telemetry.separating_fighter_count > 0;
        saw_avoidance = saw_avoidance || telemetry.avoiding_fighter_count > 0;
    };
    harness.timeline.finish_at(1.0);
    tests::expect_true(harness.run_until_timeline_finished(2.0),
                       "Clear-navigation timeline completes");
    tests::expect_true(recorded_first, "Clear-path fighter spawned");
    tests::expect_true(scanned_before_first_movement,
                       "New fighter scans obstacles before its first movement");
    tests::expect_true(last_location.X > first_location.X + 1000.f,
                       "Clear-path fighter advances directly");
    tests::expect_true(std::abs(last_location.Y - first_location.Y) < 1.f &&
                           std::abs(last_location.Z - first_location.Z) < 1.f,
                       "Clear path has no lateral steering");
    tests::expect_true(!saw_separation, "Clear path never applies separation");
    tests::expect_true(!saw_avoidance, "Clear path never applies hard avoidance");
}

void run_worldless_fighter_separation(tests::SimulationFixture const& config) {
    auto data{make_fighter_navigation_test_data(
        config,
        {Transform3d{.location = ml::Vector3d{3000.f, 6950.f, 0.f}},
         Transform3d{.location = ml::Vector3d{3000.f, 7050.f, 0.f}}},
        Vector3f{{-20000.f, -7000.f, 0.f}},
        Vector3f{{20000.f, 0.f, 0.f}})};
    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& fighters{harness.get_simulation().get_fighters()};
    float initial_distance{};
    float final_distance{};
    bool saw_separation{};
    harness.on_end_tick = [&](LevelSim&) {
        auto const locations{fighters.get_locations()};
        if (locations.num() != 2) {
            return;
        }
        final_distance = tests::distance(locations[0], locations[1]);
        if (initial_distance == 0.f) {
            initial_distance = final_distance;
        }
        saw_separation =
            saw_separation || fighters.get_navigation_telemetry().separating_fighter_count == 2;
    };
    harness.timeline.finish_at(1.0);
    tests::expect_true(harness.run_until_timeline_finished(2.0),
                       "Fighter-separation timeline completes");
    tests::expect_true(saw_separation, "Both close fighters apply separation");
    tests::expect_true(final_distance > initial_distance + 500.f,
                       "Close fighters diverge instead of remaining clustered");
}

struct FighterClusterMetrics {
    std::int32_t overlapping_pair_count{};
    std::int32_t separated_fighter_count{};
    float median_centroid_distance{};
    float median_nearest_neighbour_distance{};
    bool finite{true};
};

auto measure_fighter_cluster(std::span<Vector3f const> const locations,
                             float const collision_distance) -> FighterClusterMetrics {
    FighterClusterMetrics result;
    if (locations.empty()) {
        return result;
    }

    Vector3f centroid{Vector3f{}};
    for (auto const location : locations) {
        result.finite =
            result.finite && !(!std::isfinite(location.X) || !std::isfinite(location.Y) ||
                               !std::isfinite(location.Z));
        centroid += location;
    }
    centroid /= static_cast<float>(static_cast<std::int32_t>(locations.size()));

    std::vector<float> centroid_distances{};
    std::vector<float> nearest_neighbour_distances{};
    centroid_distances.reserve(static_cast<std::int32_t>(locations.size()));
    nearest_neighbour_distances.reserve(static_cast<std::int32_t>(locations.size()));
    for (std::int32_t i{}; i < static_cast<std::int32_t>(locations.size()); ++i) {
        auto nearest_distance{std::numeric_limits<float>::max()};
        centroid_distances.push_back(tests::distance(locations[i], centroid));
        for (std::int32_t j{}; j < static_cast<std::int32_t>(locations.size()); ++j) {
            if (i == j) {
                continue;
            }
            auto const distance{tests::distance(locations[i], locations[j])};
            nearest_distance = std::min(nearest_distance, distance);
            if (j > i && distance < collision_distance) {
                ++result.overlapping_pair_count;
            }
        }
        if (nearest_distance >= collision_distance) {
            ++result.separated_fighter_count;
        }
        nearest_neighbour_distances.push_back(nearest_distance);
    }

    std::ranges::sort(centroid_distances);
    std::ranges::sort(nearest_neighbour_distances);
    result.median_centroid_distance = centroid_distances[centroid_distances.size() / 2];
    result.median_nearest_neighbour_distance =
        nearest_neighbour_distances[nearest_neighbour_distances.size() / 2];
    return result;
}

struct DenseNavigationResult {
    std::vector<Vector3f> locations{};
    std::int32_t query_count{};
    float collision_distance{};
    bool saw_immediate_risk{};
    bool timeline_completed{};
};

auto run_dense_navigation_fixture(tests::SimulationFixture const& config,
                                  std::int32_t const fighter_count) -> DenseNavigationResult {
    std::vector<Transform3d> spawn_slots{};
    spawn_slots.assign(fighter_count, Transform3d{.location = ml::Vector3d{3000.f, 7000.f, 0.f}});
    auto data{make_fighter_navigation_test_data(config,
                                                std::move(spawn_slots),
                                                Vector3f{{-20000.f, -7000.f, 0.f}},
                                                Vector3f{{20000.f, 0.f, 0.f}})};
    DenseNavigationResult result;
    result.collision_distance =
        collision::get_entity_radius(data.entity_bounds, collision::EntityAABBs::fighter_index) *
        2.f;
    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& fighters{harness.get_simulation().get_fighters()};
    harness.on_end_tick = [&](LevelSim&) {
        auto const& telemetry{fighters.get_navigation_telemetry()};
        result.query_count += telemetry.separation_query_count;
        result.saw_immediate_risk = result.saw_immediate_risk || telemetry.immediate_risk_count > 0;
    };
    harness.timeline.finish_at(2.0);
    result.timeline_completed = harness.run_until_timeline_finished(2.5);
    auto const locations{fighters.get_locations()};
    result.locations.reserve(locations.num());
    for (std::int32_t i{}; i < locations.num(); ++i) {
        result.locations.push_back(locations[i]);
    }
    return result;
}

void run_worldless_fighter_dense_determinism(tests::SimulationFixture const& config) {
    auto const first_result{run_dense_navigation_fixture(config, 8)};
    auto const second_result{run_dense_navigation_fixture(config, 8)};
    auto const& first{first_result.locations};
    auto const& second{second_result.locations};
    tests::expect_true(first_result.timeline_completed, "First dense fixture timeline completes");
    tests::expect_true(second_result.timeline_completed, "Second dense fixture timeline completes");
    tests::expect_equal(
        8, static_cast<std::int32_t>(first.size()), "Dense fixture retains all fighters");
    tests::expect_equal(static_cast<std::int32_t>(first.size()),
                        static_cast<std::int32_t>(second.size()),
                        "Deterministic fixtures have equal counts");
    tests::expect_equal(first_result.collision_distance,
                        second_result.collision_distance,
                        "Deterministic fixtures use equal collision distances");

    for (std::int32_t i{}; i < static_cast<std::int32_t>(first.size()); ++i) {
        tests::expect_true(!(!std::isfinite(first[i].X) || !std::isfinite(first[i].Y) ||
                             !std::isfinite(first[i].Z)),
                           "Dense fighter location is finite",
                           i);
        if ((i >= 0 && static_cast<std::size_t>(i) < second.size())) {
            tests::expect_true((std::abs(first[i].X - second[i].X) <= 0.f &&
                                std::abs(first[i].Y - second[i].Y) <= 0.f &&
                                std::abs(first[i].Z - second[i].Z) <= 0.f),
                               "Identical fixtures produce identical fighter positions",
                               i);
        }
    }
    auto const metrics{measure_fighter_cluster(first, first_result.collision_distance)};
    tests::expect_true(metrics.finite, "Every dense fighter remains finite");
    tests::expect_less_equal(
        metrics.overlapping_pair_count, 7, "At least three quarters of coincident pairs separate");
    tests::expect_less_equal(
        1, metrics.separated_fighter_count, "Dense group produces fully separated fighters");
    tests::expect_less_equal(
        750.f, metrics.median_centroid_distance, "The median fighter leaves the cluster core");
    tests::expect_less_equal(first_result.collision_distance * 0.25f,
                             metrics.median_nearest_neighbour_distance,
                             "Dense fighters establish meaningful local spacing");
    tests::expect_true(first_result.saw_immediate_risk && second_result.saw_immediate_risk,
                       "Coincident groups enter the immediate-risk tier");
    tests::expect_equal(first_result.query_count,
                        second_result.query_count,
                        "Identical fixtures schedule the same number of queries");
}

void run_worldless_fighter_large_cluster(tests::SimulationFixture const& config) {
    constexpr std::int32_t fighter_count{48};
    auto const result{run_dense_navigation_fixture(config, fighter_count)};
    auto const metrics{measure_fighter_cluster(result.locations, result.collision_distance)};
    auto const initial_pair_count{fighter_count * (fighter_count - 1) / 2};

    tests::expect_true(result.timeline_completed, "Large-cluster timeline completes");
    tests::expect_equal(fighter_count,
                        static_cast<std::int32_t>(result.locations.size()),
                        "Large cluster retains every fighter");
    tests::expect_true(metrics.finite, "Large-cluster positions remain finite");
    tests::expect_less_equal(metrics.overlapping_pair_count,
                             initial_pair_count / 4,
                             "Large cluster removes at least three quarters of initial overlaps");
    tests::expect_less_equal(
        750.f, metrics.median_centroid_distance, "Large cluster expands beyond its original core");
    tests::expect_less_equal(result.collision_distance * 0.25f,
                             metrics.median_nearest_neighbour_distance,
                             "Large cluster establishes meaningful local spacing");
    tests::expect_true(result.query_count > 0 && result.saw_immediate_risk,
                       "Large cluster exercises immediate-risk navigation");
}

void run_worldless_fighter_hard_avoidance_authority(tests::SimulationFixture const& config) {
    Vector3f const obstacle_min{{-1000.f, -1000.f, -1500.f}};
    Vector3f const obstacle_max{{1000.f, 1000.f, 1500.f}};
    auto data{make_fighter_navigation_test_data(
        config,
        {Transform3d{.location = ml::Vector3d{3000.f, 31500.f, 0.f}},
         Transform3d{.location = ml::Vector3d{3000.f, 31600.f, 0.f}}},
        Vector3f{{-15000.f, -30000.f, 0.f}},
        Vector3f{{15000.f, 0.f, 0.f}})};
    data.fighters.separation_strength = 3.f;
    data.static_bounds.add_defaulted(1);
    collision::set(data.static_bounds, 0, obstacle_min, obstacle_max);
    auto const fighter_radius{
        collision::get_entity_radius(data.entity_bounds, collision::EntityAABBs::fighter_index)};
    auto const clearance{fighter_radius + data.fighters.avoidance_clearance_buffer};
    Vector3f const clearance_extent{{clearance, clearance, clearance}};
    auto const expanded_min{obstacle_min - clearance_extent};
    auto const expanded_max{obstacle_max + clearance_extent};

    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& fighters{harness.get_simulation().get_fighters()};
    bool entered_obstacle{};
    bool saw_separation{};
    bool saw_avoidance{};
    std::int32_t avoidance_transitions{};
    bool previously_avoiding{};
    float maximum_x{-std::numeric_limits<float>::max()};
    harness.on_end_tick = [&](LevelSim&) {
        auto const locations{fighters.get_locations()};
        for (std::int32_t i{}; i < locations.num(); ++i) {
            auto const location{locations[i]};
            entered_obstacle =
                entered_obstacle || (location.X >= expanded_min.X && location.X <= expanded_max.X &&
                                     location.Y >= expanded_min.Y && location.Y <= expanded_max.Y &&
                                     location.Z >= expanded_min.Z && location.Z <= expanded_max.Z);
            maximum_x = std::max(maximum_x, location.X);
        }
        auto const& telemetry{fighters.get_navigation_telemetry()};
        saw_separation = saw_separation || telemetry.separating_fighter_count > 0;
        auto const avoiding{telemetry.avoiding_fighter_count > 0};
        saw_avoidance = saw_avoidance || avoiding;
        if (avoiding != previously_avoiding) {
            ++avoidance_transitions;
            previously_avoiding = avoiding;
        }
    };
    harness.timeline.finish_at(10.0);
    tests::expect_true(harness.run_until_timeline_finished(11.0),
                       "Hard-authority timeline completes");
    tests::expect_true(saw_separation, "Obstacle fixture applies soft separation");
    tests::expect_true(saw_avoidance, "Hard avoidance overrides unsafe soft steering");
    tests::expect_true(!entered_obstacle, "Soft steering never enters obstacle clearance");
    tests::expect_true(maximum_x > expanded_max.X, "Fighters progress beyond the obstacle");
    tests::expect_less_equal(
        avoidance_transitions, 6, "Held hard steering does not flap pathologically");
}

void run_worldless_fighter_navigation_frequency(tests::SimulationFixture const& config) {
    auto data{make_fighter_navigation_test_data(
        config,
        {Transform3d{.location = ml::Vector3d{3000.f, 7000.f, 0.f}}},
        Vector3f{{-20000.f, -7000.f, 0.f}},
        Vector3f{{20000.f, 0.f, 0.f}})};
    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& fighters{harness.get_simulation().get_fighters()};
    std::int32_t clear_queries{};
    bool saw_clear{};
    harness.on_end_tick = [&](LevelSim&) {
        auto const& telemetry{fighters.get_navigation_telemetry()};
        clear_queries += telemetry.separation_query_count;
        saw_clear = saw_clear || telemetry.clear_risk_count > 0;
    };
    harness.timeline.finish_at(1.5);
    tests::expect_true(harness.run_until_timeline_finished(2.0),
                       "Clear-frequency timeline completes");

    auto const dense_result{run_dense_navigation_fixture(config, 8)};
    tests::expect_true(saw_clear, "Clear fighter demotes to the clear-risk tier");
    tests::expect_true(dense_result.saw_immediate_risk,
                       "Clustered fighters promote to immediate risk");
    tests::expect_true(dense_result.query_count > clear_queries * 4,
                       "Risky fighters are evaluated materially more frequently");
}

void run_worldless_fighter_attack(tests::SimulationFixture const& config) {
    auto data{tests::make_simulation_data(config)};
    data.fighters.laser.projectile_speed = 20000.f;
    data.fighters.laser.max_distance = 25000.f;
    tests::add_capital_spawn(data, Vector3f{{-22020.f, 2170.f, 4360.f}}, Team::Green, 1, 0.f, 60.f);
    tests::add_capital_spawn(data, Vector3f{{17030.f, 2170.f, 4360.f}}, Team::Red, 0, 60.f, 60.f);

    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    auto const& fighters{harness.get_simulation().get_fighters()};
    auto const enemy{capitals.get_handle(1)};
    struct Sample {
        std::int32_t enemy_health{};
        std::vector<Team> fighter_teams{};
    };
    ml::TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](LevelSim&) {
        Sample sample{.enemy_health = capitals.get_health(enemy)};
        for (auto const team : fighters.get_teams()) {
            sample.fighter_teams.push_back(team);
        }
        samples.add(harness.get_time(), std::move(sample));
    };
    harness.timeline.finish_at(11.0);
    tests::expect_true(harness.run_until_timeline_finished(12.0),
                       "Fighter-attack timeline completes");
    tests::expect_true(!samples.is_empty(), "Fighter-attack samples are recorded");
    if (samples.is_empty()) {
        return;
    }

    auto const& before{samples.nearest_value(1.0)};
    auto const& after{samples.nearest_value(11.0)};
    tests::expect_greater(static_cast<std::int32_t>(before.fighter_teams.size()),
                          std::int32_t{0},
                          "Hero fighters spawned");
    for (std::int32_t i{}; i < static_cast<std::int32_t>(before.fighter_teams.size()); ++i) {
        tests::expect_equal(Team::Green, before.fighter_teams[i], "Fighter is on the hero team", i);
    }
    tests::expect_true(after.enemy_health < before.enemy_health, "Enemy lost health");
}

}
