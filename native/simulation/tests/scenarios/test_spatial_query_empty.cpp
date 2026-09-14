#include "test_spatial_query_empty.h"
#include "../support/simulation_test_support.h"

#include <ioj/sim/spatial_query_manager.h>

namespace ioj::sim {
void run_worldless_spatial_query_empty(ioj::sim::tests::SimulationFixture const& config) {
    auto data{ioj::sim::tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    ioj::sim::tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto& queries{harness.get_simulation().get_spatial_query_manager()};
    std::vector<RegistryEntityHandle> handles{};
    ioj::sim::Vectors3f starts;
    ioj::sim::Vectors3f ends;
    queries.trace_line_of_sight(starts.get_const_view(), ends.get_const_view(), handles);
    std::vector<std::uint8_t> line_of_sight{};
    queries.has_line_of_sight_to_targets(
        ml::make_vector3f(0.f, 0.f, 0.f), ends.get_const_view(), handles, line_of_sight);
    std::vector<RegistryEntityHandle> results{};
    auto const count{queries.collect_non_team_entities_in_range(
        ml::make_vector3f(0.f, 0.f, 0.f), ioj::sim::Team::Blue, 1000.f, results)};
    ioj::sim::tests::expect_equal(count, 0, "Empty range query has no results");
    ioj::sim::tests::expect_true(queries.get_any_non_team_entity(ioj::sim::Team::Blue).is_null(),
                                 "Empty world has no arbitrary enemy");
}

void run_worldless_spatial_query_range(ioj::sim::tests::SimulationFixture const& config) {
    auto data{ioj::sim::tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    ioj::sim::tests::add_capital_spawn(data, ioj::sim::Vector3f{}, ioj::sim::Team::Blue);
    ioj::sim::tests::add_capital_spawn(
        data, ioj::sim::Vector3f{{500.f, 0.f, 0.f}}, ioj::sim::Team::Blue);
    ioj::sim::tests::add_capital_spawn(
        data, ioj::sim::Vector3f{{1000.f, 0.f, 0.f}}, ioj::sim::Team::Red);
    ioj::sim::tests::add_capital_spawn(
        data, ioj::sim::Vector3f{{1000.1f, 0.f, 0.f}}, ioj::sim::Team::Red);
    ioj::sim::tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const ignored_origin{harness.get_simulation().get_capital_ships().get_handle(0)};
    auto const friendly{harness.get_simulation().get_capital_ships().get_handle(1)};
    auto const boundary_enemy{harness.get_simulation().get_capital_ships().get_handle(2)};
    std::array<RegistryEntityHandle, 4> results;
    auto const count{
        harness.get_simulation().get_spatial_query_manager().collect_non_team_entities_in_range(
            ml::make_vector3f(0.f, 0.f, 0.f), ioj::sim::Team::Blue, 1000.f, results)};
    ioj::sim::tests::expect_equal(count, 1, "Only one enemy is within the inclusive radius");
    if (count == 1) {
        ioj::sim::tests::expect_equal(boundary_enemy, results[0], "Boundary enemy is included");
    }

    auto const type_count{
        harness.get_simulation().get_spatial_query_manager().collect_entities_of_type_in_range(
            ml::make_vector3f(0.f, 0.f, 0.f),
            ioj::sim::EntityType::CapitalShip,
            1000.f,
            ignored_origin,
            results)};
    ioj::sim::tests::expect_equal(
        type_count, 2, "Type-filtered query ignores self and includes two capitals");
    if (type_count == 2) {
        ioj::sim::tests::expect_equal(friendly, results[0], "Type-filtered order is deterministic");
        ioj::sim::tests::expect_equal(
            boundary_enemy, results[1], "Type-filtered query includes the boundary entity");
    }
}

}
