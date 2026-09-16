#include "test_spatial_query_empty.h"
#include "../support/simulation_test_support.h"

#include <ioj/sim/spatial_query_manager.h>

namespace ioj::sim {
void run_worldless_spatial_query_empty(tests::SimulationFixture const& config) {
    auto data{tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& queries{harness.get_simulation().get_spatial_query_manager()};
    std::vector<RegistryEntityHandle> handles{};
    Vectors3f starts;
    Vectors3f ends;
    queries.trace_line_of_sight(starts.get_const_view(), ends.get_const_view(), handles);
    std::vector<std::uint8_t> line_of_sight{};
    queries.has_line_of_sight_to_targets(
        ml::make_vector3f(0.f, 0.f, 0.f), ends.get_const_view(), handles, line_of_sight);
    std::vector<RegistryEntityHandle> results{};
    auto const count{queries.collect_non_team_entities_in_range(
        ml::make_vector3f(0.f, 0.f, 0.f), Team::Blue, 1000.f, results)};
    tests::expect_equal(count, 0, "Empty range query has no results");
    tests::expect_true(queries.get_any_non_team_entity(Team::Blue).is_null(),
                       "Empty world has no arbitrary enemy");
}

void run_worldless_spatial_query_range(tests::SimulationFixture const& config) {
    auto data{tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    tests::add_capital_spawn(data, Vector3f{}, Team::Blue);
    tests::add_capital_spawn(data, Vector3f{{500.f, 0.f, 0.f}}, Team::Blue);
    tests::add_capital_spawn(data, Vector3f{{1000.f, 0.f, 0.f}}, Team::Red);
    tests::add_capital_spawn(data, Vector3f{{1000.1f, 0.f, 0.f}}, Team::Red);
    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const ignored_origin{harness.get_simulation().get_capital_ships().get_handle(0)};
    auto const friendly{harness.get_simulation().get_capital_ships().get_handle(1)};
    auto const boundary_enemy{harness.get_simulation().get_capital_ships().get_handle(2)};
    std::array<RegistryEntityHandle, 4> results;
    auto const count{
        harness.get_simulation().get_spatial_query_manager().collect_non_team_entities_in_range(
            ml::make_vector3f(0.f, 0.f, 0.f), Team::Blue, 1000.f, results)};
    tests::expect_equal(count, 1, "Only one enemy is within the inclusive radius");
    if (count == 1) {
        tests::expect_equal(boundary_enemy, results[0], "Boundary enemy is included");
    }

    std::array<EntityUniqueId, 4> ids;
    auto const type_count{
        harness.get_simulation().get_spatial_query_manager().collect_entities_of_type_in_range(
            ml::make_vector3f(0.f, 0.f, 0.f),
            EntityType::CapitalShip,
            1000.f,
            harness.get_registry().get_current_id(ignored_origin),
            ids)};
    tests::expect_equal(
        type_count, 2, "Type-filtered query ignores self and includes two capitals");
    if (type_count == 2) {
        tests::expect_true(harness.get_registry().get_current_id(friendly) == ids[0],
                           "Type-filtered order is deterministic");
        tests::expect_true(harness.get_registry().get_current_id(boundary_enemy) == ids[1],
                           "Type-filtered query includes the boundary entity");
    }
}

}
