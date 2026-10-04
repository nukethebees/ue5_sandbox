#include "test_spatial_query_empty.h"

#include "../support/collision_agent_storage.h"
#include "../support/simulation_test_support.h"
#include <ioj/sim/spatial_query_manager.h>

#include <sandbox/core/frame_memory_resource.h>

#include <array>

namespace ioj::sim {
void run_worldless_spatial_query_empty(tests::SimulationFixture const& config) {
    alignas(ml::FrameMemoryResource::backing_alignment) std::array<std::byte, 64 * 1024>
        query_backing{};
    ml::FrameMemoryResource query_memory{query_backing};
    ml::FrameScratchScope query_scope{query_memory};

    auto data{tests::make_simulation_data(config)};
    tests::CollisionAgentStorage owners;
    SpatialQueryManager queries{owners.entity_tables};
    queries.initialise(data.grid_geometry, data.entity_bounds);
    owners.publish();
    owners.refresh(queries);
    std::vector<EntityUniqueId> ids{};
    Vectors3f starts;
    Vectors3f ends;
    queries.trace_line_of_sight(starts.get_const_view(), ends.get_const_view(), ids, &query_memory);
    std::vector<LineQueryResult> line_of_sight{};
    queries.has_line_of_sight_to_targets(
        ml::make_vector3f(0.f, 0.f, 0.f), ends.get_const_view(), {}, line_of_sight, &query_memory);
    std::vector<EntityUniqueId> results{};
    auto const count{queries.collect_non_team_entities_in_range(
        ml::make_vector3f(0.f, 0.f, 0.f), Team::Blue, 1000.f, results, &query_memory)};
    EXPECT_EQ(count, 0) << "Empty range query has no results";
    EXPECT_TRUE(!queries.get_any_non_team_entity(Team::Blue, EntityType::CapitalShip).is_valid())
        << "Empty world has no arbitrary enemy";
}

void run_worldless_spatial_query_range(tests::SimulationFixture const& config) {
    alignas(ml::FrameMemoryResource::backing_alignment) std::array<std::byte, 64 * 1024>
        query_backing{};
    ml::FrameMemoryResource query_memory{query_backing};
    ml::FrameScratchScope query_scope{query_memory};

    auto data{tests::make_simulation_data(config)};
    tests::CollisionAgentStorage owners;
    auto const ignored_origin{owners.spawn(EntityType::CapitalShip, {}, {}, 100, Team::Blue)};
    auto const friendly{
        owners.spawn(EntityType::CapitalShip, {{500.f, 0.f, 0.f}}, {}, 100, Team::Blue)};
    auto const boundary_enemy{
        owners.spawn(EntityType::CapitalShip, {{1000.f, 0.f, 0.f}}, {}, 100, Team::Red)};
    owners.spawn(EntityType::CapitalShip, {{1000.1f, 0.f, 0.f}}, {}, 100, Team::Red);
    SpatialQueryManager queries{owners.entity_tables};
    queries.initialise(data.grid_geometry, data.entity_bounds);
    owners.publish();
    owners.refresh(queries);
    std::array<EntityUniqueId, 4> results;
    auto const count{queries.collect_non_team_entities_in_range(
        ml::make_vector3f(0.f, 0.f, 0.f), Team::Blue, 1000.f, results, &query_memory)};
    EXPECT_EQ(count, 1) << "Only one enemy is within the inclusive radius";
    if (count == 1) {
        EXPECT_EQ(boundary_enemy, results[0]) << "Boundary enemy is included";
    }

    std::array<EntityUniqueId, 4> ids;
    auto const type_count{
        queries.collect_entities_of_type_in_range(ml::make_vector3f(0.f, 0.f, 0.f),
                                                  EntityType::CapitalShip,
                                                  1000.f,
                                                  ignored_origin,
                                                  ids,
                                                  &query_memory)};
    EXPECT_EQ(type_count, 2) << "Type-filtered query ignores self and includes two capitals";
    if (type_count == 2) {
        EXPECT_TRUE(friendly == ids[0]) << "Type-filtered order is deterministic";
        EXPECT_TRUE(boundary_enemy == ids[1]) << "Type-filtered query includes the boundary entity";
    }
}

}
