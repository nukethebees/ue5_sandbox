#include "test_fighter_attack_scenario.h"

#include <SandboxCore/soa_rotator_utils.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestEntityRegistryData.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersConfig.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersSimulation.h>
#include <SpaceGame/simulation/EntityWorldBounds.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxTests/support/level_checks.h>
#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/time_series_test_data.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>

#include <limits>

namespace ml {
void run_worldless_fighter_obstacle_avoidance(FAutomationTestBase& test,
                                              FSoftTestAssertions& checks,
                                              USpaceGameLevelConfig const& config) {
    constexpr float source_x{-15000.f};
    constexpr float target_x{15000.f};
    FVector3f const obstacle_min{-1000.f, -1500.f, -1500.f};
    FVector3f const obstacle_max{1000.f, 1500.f, 1500.f};

    auto data{make_worldless_simulation_test_data(config)};
    data.fighters.speed = 4000.f;
    data.fighters.avoidance_update_frequency = 5.f;
    data.fighters.avoidance_lookahead_time = 1.f;
    data.fighters.avoidance_clearance_buffer = 100.f;
    data.capital_ships.fighter_spawn_slots = 1;
    data.capital_ships.fighter_spawn_slots_relative_transforms = {
        FTransform{FVector{3000.f, 7000.f, 0.f}}};
    data.static_bounds.add_defaulted(1);
    data.static_bounds.mins.set(0, obstacle_min);
    data.static_bounds.maxes.set(0, obstacle_max);
    add_worldless_capital_spawn(
        data, FVector3f{source_x, -7000.f, 0.f}, ETestTeam::Green, 1, 0.f, 60.f, 100000);
    add_worldless_capital_spawn(
        data, FVector3f{target_x, 0.f, 0.f}, ETestTeam::Red, 0, 60.f, 60.f, 100000);

    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto const* fighters{harness.get_simulation().get_capital_ship_fighters()};
    auto const clearance{fighters->collision_radius + 100.f};
    auto const expanded_min{obstacle_min - FVector3f{clearance, clearance, clearance}};
    auto const expanded_max{obstacle_max + FVector3f{clearance, clearance, clearance}};
    bool fighter_spawned{};
    bool entered_expanded_obstacle{};
    float maximum_lateral_distance{};
    float maximum_x{-std::numeric_limits<float>::infinity()};

    harness.on_end_tick = [&](FLevelSimulation&) {
        auto const locations{fighters->get_locations()};
        if (locations.num() == 0) {
            return;
        }

        fighter_spawned = true;
        auto const location{ml::get_vector3f(locations, 0)};
        maximum_lateral_distance =
            FMath::Max(maximum_lateral_distance, FVector2f{location.Y, location.Z}.Length());
        maximum_x = FMath::Max(maximum_x, location.X);
        entered_expanded_obstacle = entered_expanded_obstacle ||
                                    (location.X >= expanded_min.X && location.X <= expanded_max.X &&
                                     location.Y >= expanded_min.Y && location.Y <= expanded_max.Y &&
                                     location.Z >= expanded_min.Z && location.Z <= expanded_max.Z);
    };
    harness.timeline.finish_at(14.0);
    test.TestTrue(TEXT("Fighter obstacle-avoidance timeline completes"),
                  harness.run_until_timeline_finished(15.0));

    checks.is_true(fighter_spawned, TEXT("Avoidance fighter spawned"));
    checks.is_true(!entered_expanded_obstacle,
                   TEXT("Fighter remains outside obstacle clearance bounds"));
    checks.is_true(maximum_lateral_distance > expanded_max.Y,
                   TEXT("Fighter steers visibly around the obstacle"));
    checks.is_true(maximum_x > expanded_max.X, TEXT("Fighter progresses past the obstacle"));
}

auto make_fighter_navigation_test_data(USpaceGameLevelConfig const& config,
                                       TArray<FTransform> spawn_slots,
                                       FVector3f const source_location,
                                       FVector3f const target_location)
    -> FLevelSimulationInitData {
    auto data{make_worldless_simulation_test_data(config)};
    data.fighters.speed = 4000.f;
    data.fighters.avoidance_lookahead_time = 1.f;
    data.fighters.laser.max_distance = 2000.f;
    data.fighters.attack_engagement_threshold = 1000000.f;
    data.fighters.attack_distance_band.minimum_ratio = 0.f;
    data.fighters.attack_distance_band.desired_ratio = 0.f;
    data.fighters.attack_distance_band.maximum_ratio = 0.f;
    data.fighters.separation_radius = 1500.f;
    data.fighters.separation_strength = 1.5f;
    data.capital_ships.fighter_spawn_slots = spawn_slots.Num();
    data.capital_ships.fighter_spawn_slots_relative_transforms = MoveTemp(spawn_slots);
    add_worldless_capital_spawn(data, source_location, ETestTeam::Green, 1, 0.f, 60.f, 100000);
    add_worldless_capital_spawn(
        data, target_location, ETestTeam::Red, INDEX_NONE, 60.f, 60.f, 100000);
    return data;
}

void run_worldless_fighter_capital_obstruction(FAutomationTestBase& test,
                                               FSoftTestAssertions& checks,
                                               USpaceGameLevelConfig const& config) {
    FVector3f const source{-18000.f, -7000.f, 0.f};
    FVector3f const obstacle{0.f, 0.f, 0.f};
    FVector3f const target{18000.f, 0.f, 0.f};
    auto data{make_worldless_simulation_test_data(config)};
    data.fighters.speed = 4000.f;
    data.fighters.avoidance_lookahead_time = 1.f;
    data.fighters.laser.max_distance = 2000.f;
    data.fighters.attack_engagement_threshold = 1000000.f;
    data.fighters.attack_distance_band.minimum_ratio = 0.f;
    data.fighters.attack_distance_band.desired_ratio = 0.f;
    data.fighters.attack_distance_band.maximum_ratio = 0.f;
    data.capital_ships.fighter_spawn_slots = 1;
    data.capital_ships.fighter_spawn_slots_relative_transforms = {
        FTransform{FVector{3000.f, 7000.f, 0.f}}};
    add_worldless_capital_spawn(data, source, ETestTeam::Green, 2, 0.f, 60.f, 100000);
    add_worldless_capital_spawn(data, obstacle, ETestTeam::Green, INDEX_NONE, 60.f, 60.f, 100000);
    add_worldless_capital_spawn(data, target, ETestTeam::Red, INDEX_NONE, 60.f, 60.f, 100000);
    FRotator3f const obstacle_rotation{0.f, 35.f, 0.f};
    ml::assign(data.capital_spawns.rotations, 1, obstacle_rotation);

    auto const obstacle_bounds{ioj::make_entity_world_bounds(
        data.entity_bounds, ioj::FEntityAABBs::capital_ship_index, obstacle, obstacle_rotation)};
    auto const capital_half_extent{obstacle_bounds.GetExtent()};
    auto const clearance{data.fighter_radius + data.fighters.avoidance_clearance_buffer};
    FVector3f const clearance_extent{clearance, clearance, clearance};
    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto const* fighters{harness.get_simulation().get_capital_ship_fighters()};
    auto const expanded_min{obstacle_bounds.Min - clearance_extent};
    auto const expanded_max{obstacle_bounds.Max + clearance_extent};
    bool entered_obstacle{};
    bool saw_avoidance{};
    float maximum_lateral_distance{};
    float maximum_x{-TNumericLimits<float>::Max()};
    harness.on_end_tick = [&](FLevelSimulation&) {
        auto const locations{fighters->get_locations()};
        if (locations.num() == 0) {
            return;
        }
        auto const location{ml::get_vector3f(locations, 0)};
        entered_obstacle =
            entered_obstacle || (location.X >= expanded_min.X && location.X <= expanded_max.X &&
                                 location.Y >= expanded_min.Y && location.Y <= expanded_max.Y &&
                                 location.Z >= expanded_min.Z && location.Z <= expanded_max.Z);
        maximum_lateral_distance =
            FMath::Max(maximum_lateral_distance, FVector2f{location.Y, location.Z}.Length());
        maximum_x = FMath::Max(maximum_x, location.X);
        saw_avoidance =
            saw_avoidance || fighters->get_navigation_telemetry().avoiding_fighter_count > 0;
    };
    harness.timeline.finish_at(14.0);
    test.TestTrue(TEXT("Capital-obstruction timeline completes"),
                  harness.run_until_timeline_finished(15.0));
    checks.is_true(!entered_obstacle, TEXT("Fighter remains outside capital clearance bounds"));
    checks.is_true(saw_avoidance, TEXT("Fighter selects hard avoidance around the capital"));
    checks.is_true(maximum_lateral_distance > capital_half_extent.Y,
                   TEXT("Fighter travels laterally around the capital"));
    checks.is_true(maximum_x > expanded_max.X, TEXT("Fighter progresses beyond the capital"));
}

void run_worldless_fighter_clear_navigation(FAutomationTestBase& test,
                                            FSoftTestAssertions& checks,
                                            USpaceGameLevelConfig const& config) {
    auto data{make_fighter_navigation_test_data(config,
                                                {FTransform{FVector{3000.f, 7000.f, 0.f}}},
                                                FVector3f{-20000.f, -7000.f, 0.f},
                                                FVector3f{20000.f, 0.f, 0.f})};
    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto const* fighters{harness.get_simulation().get_capital_ship_fighters()};
    FVector3f first_location{FVector3f::ZeroVector};
    FVector3f last_location{FVector3f::ZeroVector};
    bool recorded_first{};
    bool recorded_first_movement{};
    bool scanned_before_first_movement{};
    bool saw_separation{};
    bool saw_avoidance{};
    harness.on_end_tick = [&](FLevelSimulation&) {
        auto const locations{fighters->get_locations()};
        if (locations.num() == 0) {
            return;
        }
        last_location = ml::get_vector3f(locations, 0);
        if (!recorded_first) {
            first_location = last_location;
            recorded_first = true;
        }
        auto const& telemetry{fighters->get_navigation_telemetry()};
        if (!recorded_first_movement && !last_location.Equals(first_location)) {
            recorded_first_movement = true;
            scanned_before_first_movement = telemetry.hard_trace_count > 0;
        }
        saw_separation = saw_separation || telemetry.separating_fighter_count > 0;
        saw_avoidance = saw_avoidance || telemetry.avoiding_fighter_count > 0;
    };
    harness.timeline.finish_at(1.0);
    test.TestTrue(TEXT("Clear-navigation timeline completes"),
                  harness.run_until_timeline_finished(2.0));
    checks.is_true(recorded_first, TEXT("Clear-path fighter spawned"));
    checks.is_true(scanned_before_first_movement,
                   TEXT("New fighter scans obstacles before its first movement"));
    checks.is_true(last_location.X > first_location.X + 1000.f,
                   TEXT("Clear-path fighter advances directly"));
    checks.is_true(FMath::Abs(last_location.Y - first_location.Y) < 1.f &&
                       FMath::Abs(last_location.Z - first_location.Z) < 1.f,
                   TEXT("Clear path has no lateral steering"));
    checks.is_true(!saw_separation, TEXT("Clear path never applies separation"));
    checks.is_true(!saw_avoidance, TEXT("Clear path never applies hard avoidance"));
}

void run_worldless_fighter_separation(FAutomationTestBase& test,
                                      FSoftTestAssertions& checks,
                                      USpaceGameLevelConfig const& config) {
    auto data{make_fighter_navigation_test_data(
        config,
        {FTransform{FVector{3000.f, 6950.f, 0.f}}, FTransform{FVector{3000.f, 7050.f, 0.f}}},
        FVector3f{-20000.f, -7000.f, 0.f},
        FVector3f{20000.f, 0.f, 0.f})};
    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto const* fighters{harness.get_simulation().get_capital_ship_fighters()};
    float initial_distance{};
    float final_distance{};
    bool saw_separation{};
    harness.on_end_tick = [&](FLevelSimulation&) {
        auto const locations{fighters->get_locations()};
        if (locations.num() != 2) {
            return;
        }
        final_distance =
            FVector3f::Dist(ml::get_vector3f(locations, 0), ml::get_vector3f(locations, 1));
        if (initial_distance == 0.f) {
            initial_distance = final_distance;
        }
        saw_separation =
            saw_separation || fighters->get_navigation_telemetry().separating_fighter_count == 2;
    };
    harness.timeline.finish_at(1.0);
    test.TestTrue(TEXT("Fighter-separation timeline completes"),
                  harness.run_until_timeline_finished(2.0));
    checks.is_true(saw_separation, TEXT("Both close fighters apply separation"));
    checks.is_true(final_distance > initial_distance + 500.f,
                   TEXT("Close fighters diverge instead of remaining clustered"));
}

struct FFighterClusterMetrics {
    int32 overlapping_pair_count{};
    int32 separated_fighter_count{};
    float median_centroid_distance{};
    float median_nearest_neighbour_distance{};
    bool finite{true};
};

auto measure_fighter_cluster(TConstArrayView<FVector3f> const locations,
                             float const collision_distance) -> FFighterClusterMetrics {
    FFighterClusterMetrics result;
    if (locations.IsEmpty()) {
        return result;
    }

    FVector3f centroid{FVector3f::ZeroVector};
    for (auto const location : locations) {
        result.finite = result.finite && !location.ContainsNaN();
        centroid += location;
    }
    centroid /= static_cast<float>(locations.Num());

    TArray<float> centroid_distances;
    TArray<float> nearest_neighbour_distances;
    centroid_distances.Reserve(locations.Num());
    nearest_neighbour_distances.Reserve(locations.Num());
    for (int32 i{}; i < locations.Num(); ++i) {
        auto nearest_distance{TNumericLimits<float>::Max()};
        centroid_distances.Add(FVector3f::Dist(locations[i], centroid));
        for (int32 j{}; j < locations.Num(); ++j) {
            if (i == j) {
                continue;
            }
            auto const distance{FVector3f::Dist(locations[i], locations[j])};
            nearest_distance = FMath::Min(nearest_distance, distance);
            if (j > i && distance < collision_distance) {
                ++result.overlapping_pair_count;
            }
        }
        if (nearest_distance >= collision_distance) {
            ++result.separated_fighter_count;
        }
        nearest_neighbour_distances.Add(nearest_distance);
    }

    centroid_distances.Sort();
    nearest_neighbour_distances.Sort();
    result.median_centroid_distance = centroid_distances[centroid_distances.Num() / 2];
    result.median_nearest_neighbour_distance =
        nearest_neighbour_distances[nearest_neighbour_distances.Num() / 2];
    return result;
}

struct FDenseNavigationResult {
    TArray<FVector3f> locations;
    int32 query_count{};
    float collision_distance{};
    bool saw_immediate_risk{};
    bool timeline_completed{};
};

auto run_dense_navigation_fixture(USpaceGameLevelConfig const& config, int32 const fighter_count)
    -> FDenseNavigationResult {
    TArray<FTransform> spawn_slots;
    spawn_slots.Init(FTransform{FVector{3000.f, 7000.f, 0.f}}, fighter_count);
    auto data{make_fighter_navigation_test_data(config,
                                                MoveTemp(spawn_slots),
                                                FVector3f{-20000.f, -7000.f, 0.f},
                                                FVector3f{20000.f, 0.f, 0.f})};
    FDenseNavigationResult result;
    result.collision_distance = data.fighter_radius * 2.f;
    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto const* fighters{harness.get_simulation().get_capital_ship_fighters()};
    harness.on_end_tick = [&](FLevelSimulation&) {
        auto const& telemetry{fighters->get_navigation_telemetry()};
        result.query_count += telemetry.separation_query_count;
        result.saw_immediate_risk = result.saw_immediate_risk || telemetry.immediate_risk_count > 0;
    };
    harness.timeline.finish_at(1.5);
    result.timeline_completed = harness.run_until_timeline_finished(2.0);
    auto const locations{fighters->get_locations()};
    result.locations.Reserve(locations.num());
    for (int32 i{}; i < locations.num(); ++i) {
        result.locations.Add(ml::get_vector3f(locations, i));
    }
    return result;
}

void run_worldless_fighter_dense_determinism(FAutomationTestBase& test,
                                             FSoftTestAssertions& checks,
                                             USpaceGameLevelConfig const& config) {
    auto const first_result{run_dense_navigation_fixture(config, 8)};
    auto const second_result{run_dense_navigation_fixture(config, 8)};
    auto const& first{first_result.locations};
    auto const& second{second_result.locations};
    test.TestTrue(TEXT("First dense fixture timeline completes"), first_result.timeline_completed);
    test.TestTrue(TEXT("Second dense fixture timeline completes"),
                  second_result.timeline_completed);
    checks.are_equal(8, first.Num(), TEXT("Dense fixture retains all fighters"));
    checks.are_equal(first.Num(), second.Num(), TEXT("Deterministic fixtures have equal counts"));
    checks.are_equal(first_result.collision_distance,
                     second_result.collision_distance,
                     TEXT("Deterministic fixtures use equal collision distances"));

    for (int32 i{}; i < first.Num(); ++i) {
        checks.is_true(!first[i].ContainsNaN(), TEXT("Dense fighter location is finite"), i);
        if (second.IsValidIndex(i)) {
            checks.is_true(first[i].Equals(second[i], 0.f),
                           TEXT("Identical fixtures produce identical fighter positions"),
                           i);
        }
    }
    auto const metrics{measure_fighter_cluster(first, first_result.collision_distance)};
    checks.is_true(metrics.finite, TEXT("Every dense fighter remains finite"));
    checks.is_less_equal_than(metrics.overlapping_pair_count,
                              7,
                              TEXT("At least three quarters of coincident pairs separate"));
    checks.is_less_equal_than(
        1, metrics.separated_fighter_count, TEXT("Dense group produces fully separated fighters"));
    checks.is_less_equal_than(750.f,
                              metrics.median_centroid_distance,
                              TEXT("The median fighter leaves the cluster core"));
    checks.is_less_equal_than(first_result.collision_distance * 0.25f,
                              metrics.median_nearest_neighbour_distance,
                              TEXT("Dense fighters establish meaningful local spacing"));
    checks.is_true(first_result.saw_immediate_risk && second_result.saw_immediate_risk,
                   TEXT("Coincident groups enter the immediate-risk tier"));
    checks.are_equal(first_result.query_count,
                     second_result.query_count,
                     TEXT("Identical fixtures schedule the same number of queries"));
}

void run_worldless_fighter_large_cluster(FAutomationTestBase& test,
                                         FSoftTestAssertions& checks,
                                         USpaceGameLevelConfig const& config) {
    constexpr int32 fighter_count{48};
    auto const result{run_dense_navigation_fixture(config, fighter_count)};
    auto const metrics{measure_fighter_cluster(result.locations, result.collision_distance)};
    auto const initial_pair_count{fighter_count * (fighter_count - 1) / 2};

    test.TestTrue(TEXT("Large-cluster timeline completes"), result.timeline_completed);
    checks.are_equal(
        fighter_count, result.locations.Num(), TEXT("Large cluster retains every fighter"));
    checks.is_true(metrics.finite, TEXT("Large-cluster positions remain finite"));
    checks.is_less_equal_than(
        metrics.overlapping_pair_count,
        initial_pair_count / 4,
        TEXT("Large cluster removes at least three quarters of initial overlaps"));
    checks.is_less_equal_than(750.f,
                              metrics.median_centroid_distance,
                              TEXT("Large cluster expands beyond its original core"));
    checks.is_less_equal_than(result.collision_distance * 0.25f,
                              metrics.median_nearest_neighbour_distance,
                              TEXT("Large cluster establishes meaningful local spacing"));
    checks.is_true(result.query_count > 0 && result.saw_immediate_risk,
                   TEXT("Large cluster exercises immediate-risk navigation"));
}

void run_worldless_fighter_hard_avoidance_authority(FAutomationTestBase& test,
                                                    FSoftTestAssertions& checks,
                                                    USpaceGameLevelConfig const& config) {
    FVector3f const obstacle_min{-1000.f, -1000.f, -1500.f};
    FVector3f const obstacle_max{1000.f, 1000.f, 1500.f};
    auto data{make_fighter_navigation_test_data(
        config,
        {FTransform{FVector{3000.f, 31500.f, 0.f}}, FTransform{FVector{3000.f, 31600.f, 0.f}}},
        FVector3f{-15000.f, -30000.f, 0.f},
        FVector3f{15000.f, 0.f, 0.f})};
    data.fighters.separation_strength = 3.f;
    data.static_bounds.add_defaulted(1);
    data.static_bounds.mins.set(0, obstacle_min);
    data.static_bounds.maxes.set(0, obstacle_max);
    auto const clearance{data.fighter_radius + data.fighters.avoidance_clearance_buffer};
    FVector3f const clearance_extent{clearance, clearance, clearance};
    auto const expanded_min{obstacle_min - clearance_extent};
    auto const expanded_max{obstacle_max + clearance_extent};

    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto const* fighters{harness.get_simulation().get_capital_ship_fighters()};
    bool entered_obstacle{};
    bool saw_separation{};
    bool saw_avoidance{};
    int32 avoidance_transitions{};
    bool previously_avoiding{};
    float maximum_x{-TNumericLimits<float>::Max()};
    harness.on_end_tick = [&](FLevelSimulation&) {
        auto const locations{fighters->get_locations()};
        for (int32 i{}; i < locations.num(); ++i) {
            auto const location{ml::get_vector3f(locations, i)};
            entered_obstacle =
                entered_obstacle || (location.X >= expanded_min.X && location.X <= expanded_max.X &&
                                     location.Y >= expanded_min.Y && location.Y <= expanded_max.Y &&
                                     location.Z >= expanded_min.Z && location.Z <= expanded_max.Z);
            maximum_x = FMath::Max(maximum_x, location.X);
        }
        auto const& telemetry{fighters->get_navigation_telemetry()};
        saw_separation = saw_separation || telemetry.separating_fighter_count > 0;
        auto const avoiding{telemetry.avoiding_fighter_count > 0};
        saw_avoidance = saw_avoidance || avoiding;
        if (avoiding != previously_avoiding) {
            ++avoidance_transitions;
            previously_avoiding = avoiding;
        }
    };
    harness.timeline.finish_at(10.0);
    test.TestTrue(TEXT("Hard-authority timeline completes"),
                  harness.run_until_timeline_finished(11.0));
    checks.is_true(saw_separation, TEXT("Obstacle fixture applies soft separation"));
    checks.is_true(saw_avoidance, TEXT("Hard avoidance overrides unsafe soft steering"));
    checks.is_true(!entered_obstacle, TEXT("Soft steering never enters obstacle clearance"));
    checks.is_true(maximum_x > expanded_max.X, TEXT("Fighters progress beyond the obstacle"));
    checks.is_less_equal_than(
        avoidance_transitions, 6, TEXT("Held hard steering does not flap pathologically"));
}

void run_worldless_fighter_navigation_frequency(FAutomationTestBase& test,
                                                FSoftTestAssertions& checks,
                                                USpaceGameLevelConfig const& config) {
    auto data{make_fighter_navigation_test_data(config,
                                                {FTransform{FVector{3000.f, 7000.f, 0.f}}},
                                                FVector3f{-20000.f, -7000.f, 0.f},
                                                FVector3f{20000.f, 0.f, 0.f})};
    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto const* fighters{harness.get_simulation().get_capital_ship_fighters()};
    int32 clear_queries{};
    bool saw_clear{};
    harness.on_end_tick = [&](FLevelSimulation&) {
        auto const& telemetry{fighters->get_navigation_telemetry()};
        clear_queries += telemetry.separation_query_count;
        saw_clear = saw_clear || telemetry.clear_risk_count > 0;
    };
    harness.timeline.finish_at(1.5);
    test.TestTrue(TEXT("Clear-frequency timeline completes"),
                  harness.run_until_timeline_finished(2.0));

    auto const dense_result{run_dense_navigation_fixture(config, 8)};
    checks.is_true(saw_clear, TEXT("Clear fighter demotes to the clear-risk tier"));
    checks.is_true(dense_result.saw_immediate_risk,
                   TEXT("Clustered fighters promote to immediate risk"));
    checks.is_true(dense_result.query_count > clear_queries * 4,
                   TEXT("Risky fighters are evaluated materially more frequently"));
}

void run_worldless_fighter_attack(FAutomationTestBase& test,
                                  FSoftTestAssertions& checks,
                                  USpaceGameLevelConfig const& config) {
    auto data{make_worldless_simulation_test_data(config)};
    data.fighters.laser.projectile_speed = 20000.f;
    data.fighters.laser.max_distance = 25000.f;
    add_worldless_capital_spawn(
        data, FVector3f{-22020.f, 2170.f, 4360.f}, ETestTeam::Green, 1, 0.f, 60.f);
    add_worldless_capital_spawn(
        data, FVector3f{17030.f, 2170.f, 4360.f}, ETestTeam::Red, 0, 60.f, 60.f);

    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto const* capitals{harness.get_simulation().get_capital_ships()};
    auto const* fighters{harness.get_simulation().get_capital_ship_fighters()};
    auto const enemy{capitals->get_handle(1)};
    struct Sample {
        int32 enemy_health{};
        TArray<ETestTeam> fighter_teams;
    };
    TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](FLevelSimulation&) {
        Sample sample{.enemy_health = capitals->get_health(enemy)};
        sample.fighter_teams.Append(fighters->get_teams());
        samples.add(harness.get_time(), MoveTemp(sample));
    };
    harness.timeline.finish_at(11.0);
    test.TestTrue(TEXT("Fighter-attack timeline completes"),
                  harness.run_until_timeline_finished(12.0));
    checks.is_true(!samples.is_empty(), TEXT("Fighter-attack samples are recorded"));
    if (samples.is_empty()) {
        return;
    }

    auto const& before{samples.nearest_value(1.0)};
    auto const& after{samples.nearest_value(11.0)};
    checks.is_greater_than(before.fighter_teams.Num(), int32{0}, TEXT("Hero fighters spawned"));
    for (int32 i{}; i < before.fighter_teams.Num(); ++i) {
        checks.are_equal(
            ETestTeam::Green, before.fighter_teams[i], TEXT("Fighter is on the hero team"), i);
    }
    checks.is_true(after.enemy_health < before.enemy_health, TEXT("Enemy lost health"));
}

FFighterAttackScenario::FFighterAttackScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {
    TestCommandBuilder.Do([this] { spawn_fixture(); });
}

void FFighterAttackScenario::on_tear_down() {
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
}

/* ------------------------------------------------------------------------------------------ */
// Setup
/* ------------------------------------------------------------------------------------------ */
void FFighterAttackScenario::spawn_fixture() {
    auto* const level_config{duplicate_level_config(context_.config, context_.orchestrator)};
    if (!checks.not_nullptr(level_config, TEXT("Level config is duplicated"))) {
        return;
    }

    auto* const fighter_config{&level_config->fighters};
    if (!checks.not_nullptr(fighter_config, TEXT("Fighter attack config is created"))) {
        return;
    }
    fighter_config->laser.projectile_speed = 20000.f;
    fighter_config->laser.max_distance = 25000.f;
    fighter_config->visual_logger_style = nullptr;
    context_.orchestrator.set_level_config(*level_config);

    auto* const hero_proxy{spawn_capital_proxy(context_.world,
                                               context_.config,
                                               checks,
                                               TEXT("hero_capital"),
                                               FVector{-22020.f, 2170.f, 4360.f})};
    auto* const enemy_proxy{spawn_capital_proxy(context_.world,
                                                context_.config,
                                                checks,
                                                TEXT("enemy_capital"),
                                                FVector{17030.f, 2170.f, 4360.f})};
    if (!checks.is_valid(hero_proxy, TEXT("Hero capital is spawned")) ||
        !checks.is_valid(enemy_proxy, TEXT("Enemy capital is spawned"))) {
        return;
    }

    hero_proxy->set_team(hero_team);
    hero_proxy->set_target_ship(enemy_proxy);
    hero_proxy->set_spawn_cooldown(60.f);
    enemy_proxy->set_team(enemy_team);
    enemy_proxy->set_target_ship(hero_proxy);
    enemy_proxy->set_initial_spawn_delay(60.f);

    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
    ATestBatchOrchestrator::on_proxy_entities_bound.AddRaw(
        this, &FFighterAttackScenario::bind_proxy_entities);
}

void FFighterAttackScenario::bind_proxy_entities(FProxyEntityMap const& proxy_entities) {
    resolve_proxy_entity_bindings(
        proxy_entities,
        {{TEXT("hero_capital"), &hero, nullptr}, {TEXT("enemy_capital"), &enemy, nullptr}},
        checks);
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
}

void FFighterAttackScenario::initial_setup_and_stimuli() {
    initialise_test_driver();
    test_driver->orchestrator.start_simulation();

    checks.is_true(test_driver->get_registry().is_valid_handle(hero), TEXT("Read hero handle"));
    checks.is_true(test_driver->get_registry().is_valid_handle(enemy), TEXT("Read enemy handle"));
    checks.is_true(hero != enemy, TEXT("Hero and enemy handles are distinct"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    reset_and_reserve_time_series(
        test_driver->orchestrator, initial_wait + fight_duration, samples);
    test_driver->orchestrator.set_end_tick_test_hook(
        FOrchestratorEndTickTestHook::CreateRaw(this, &FFighterAttackScenario::on_end_tick));
    test_driver->timeline.at(initial_wait, [this] { t_pre_fight = test_driver->get_time(); })
        .then_after(fight_duration, [this] { t_post_fight = test_driver->get_time(); });
}

/* ------------------------------------------------------------------------------------------ */
// Samples and checks
/* ------------------------------------------------------------------------------------------ */
void FFighterAttackScenario::sample_values() {
    auto const& entity_data{test_driver->get_registry().get_entity_data()};
    FSimulationSample sample{};
    sample.enemy_health = test_driver->get_capital_ships().get_health(enemy);
    sample.fighter_teams.Append(test_driver->get_capital_ship_fighters().get_teams());
    sample.radii.Append(entity_data.radii);
    samples.add(test_driver->get_time(), MoveTemp(sample));
}

void FFighterAttackScenario::on_end_tick(ATestBatchOrchestrator&) {
    sample_values();
    test_driver->advance_timeline();
}

void FFighterAttackScenario::check_fighters_team(FSimulationSample const& sample) {
    check_all_teams_are(
        sample.fighter_teams, hero_team, checks, TEXT("Fighters are on hero team."));
}

void FFighterAttackScenario::full_checks() {
    auto const& pre_fight{samples.nearest_value(t_pre_fight)};
    auto const& post_fight{samples.nearest_value(t_post_fight)};
    check_fighters_team(pre_fight);
    check_radii(TConstArrayView<float>{pre_fight.radii}, checks, 0.05f);
    check_health_decreased(
        pre_fight.enemy_health, post_fight.enemy_health, checks, TEXT("Enemy lost health"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FFighterAttackScenario::run() {
    run_until_timeline_finished(
        [this] { initial_setup_and_stimuli(); }, timeout, [this] { full_checks(); });
}
}
