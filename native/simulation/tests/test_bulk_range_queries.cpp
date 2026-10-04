#include "support/collision_agent_storage.h"
#include <ioj/sim/frame_range_query_results.h>
#include <ioj/sim/spatial_query_manager.h>
#include <ioj/sim/testing/entity_observations.h>

#include <sandbox/core/frame_memory_resource.h>
#include <sandbox/core/vector_normalization.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace ioj::sim::tests {
class BulkRangeQueries : public ::testing::Test {
  protected:
    BulkRangeQueries() {
        for (auto const type : ml::EnumTraits<EntityType>::values) {
            bounds.set_half_extents(type, {{10.f, 10.f, 10.f}});
        }
        queries.initialise({{8, 8, 8}, {{100.f, 100.f, 100.f}}}, bounds);
    }

    void rebuild() {
        owners.publish();
        owners.refresh(queries);
    }

    void request(Vector3f const origin, Team const team) {
        origins.add(origin);
        teams.add(team);
    }

    void query(float const radius, FrameRangeQueryResults& output) {
        queries.collect_non_team_entities_in_range(
            origins.get_const_view(), teams.view(), radius, output, &memory);
    }

    void expect_scalar_matches(float const radius, FrameRangeQueryResults& output) {
        query(radius, output);
        ASSERT_EQ(output.ranges.num(), origins.num());
        ASSERT_EQ(output.entities.num(), output.distances.num());
        ASSERT_EQ(output.entities.num(), output.directions.num());
        auto const locations{origins.get_const_view()};
        auto const directions{output.directions.get_const_view()};
        std::array<EntityUniqueId, 512> reference;
        alignas(ml::FrameMemoryResource::backing_alignment) std::array<std::byte, 64 * 1024>
            reference_backing{};
        ml::FrameMemoryResource reference_memory{reference_backing};
        auto const reference_view{std::span{reference}};
        std::uint32_t offset{};
        auto const request_count{origins.num()};
        for (std::uint32_t index{}; index < request_count; ++index) {
            ml::FrameScratchScope reference_scope{reference_memory};
            SCOPED_TRACE(index);
            auto const range{output.ranges[index]};
            ASSERT_EQ(range.offset, offset);
            ASSERT_LE(range.end(), output.entities.num());
            offset = range.end();
            auto const count{queries.collect_non_team_entities_in_range(
                locations[index], teams[index], radius, reference, &reference_memory)};
            // NOLINTNEXTLINE(ioj-loop-view-accessor-call) -- result count depends on this request.
            auto const expected{reference_view.first(count)};
            std::vector<EntityUniqueId> actual;
            auto const end{range.end()};
            for (auto match{range.offset}; match < end; ++match) {
                auto const id{output.entities[match]};
                actual.push_back(id);
                auto const state{observe_entity(owners, id)};
                ASSERT_TRUE(state);
                auto const delta{state->location - locations[index]};
                EXPECT_FLOAT_EQ(output.distances[match], HMM_LenV3(delta));
                auto const direction{ml::native_math::safe_normal(delta, 1.e-8f)};
                EXPECT_FLOAT_EQ(directions[match].X, direction.X);
                EXPECT_FLOAT_EQ(directions[match].Y, direction.Y);
                EXPECT_FLOAT_EQ(directions[match].Z, direction.Z);
            }
            std::ranges::sort(actual);
            std::ranges::sort(expected);
            EXPECT_TRUE(std::ranges::equal(actual, expected));
            EXPECT_EQ(std::ranges::adjacent_find(actual), actual.end());
        }
        EXPECT_EQ(offset, output.entities.num());
    }

    CollisionAgentStorage owners;
    SpatialQueryManager queries{owners.entity_tables};
    collision::EntityAABBs bounds;
    alignas(
        ml::FrameMemoryResource::backing_alignment) std::array<std::byte, 1024 * 1024> backing{};
    ml::FrameMemoryResource memory{backing};
    ml::FrameScratchScope scope{memory};
    FrameVectors3f origins{&memory};
    ml::FrameArray<Team> teams{&memory};
};

TEST_F(BulkRangeQueries, OverlappingAndDisjointRequestsMatchScalarQueriesInInputOrder) {
    for (std::uint32_t index{}; index < 180; ++index) {
        owners.spawn(EntityType::Fighter,
                     {{static_cast<float>(index % 9) * 80.f - 320.f,
                       static_cast<float>((index / 9) % 5) * 120.f - 240.f,
                       static_cast<float>(index / 45) * 110.f - 165.f}},
                     {},
                     100,
                     static_cast<Team>(index % ml::enum_count<Team>()));
    }
    owners.spawn(EntityType::CapitalShip, {});
    owners.spawn(EntityType::Turret, {}, {}, 100, Team::Red);
    owners.spawn(EntityType::PlayerShip, {}, {}, 100, Team::Green);
    owners.spawn(EntityType::TubeSpinner, {});
    rebuild();
    // Exercise identical bounds, overlapping bounds, clamped bounds, and fully outside requests.
    for (std::uint32_t index{}; index < 150; ++index) {
        request({{static_cast<float>(index % 10) * 110.f - 440.f,
                  static_cast<float>((index / 10) % 3) * 140.f - 140.f,
                  0.f}},
                static_cast<Team>(index % ml::enum_count<Team>()));
    }
    request({{2000.f, 0.f, 0.f}}, Team::White);
    FrameRangeQueryResults output{&memory};
    expect_scalar_matches(175.f, output);
}

TEST_F(BulkRangeQueries, DenseMulticellMembershipIsUniqueAndHasNo128ResultLimit) {
    constexpr std::uint32_t count{200};
    for (std::uint32_t index{}; index < count; ++index) {
        owners.spawn(EntityType::Fighter, {}, {}, 100, Team::Red);
    }
    owners.spawn(EntityType::Fighter, {}, {}, 100, Team::Blue);
    rebuild();
    request({}, Team::Blue);
    request({{1.f, 0.f, 0.f}}, Team::Blue);
    FrameRangeQueryResults output{&memory};
    expect_scalar_matches(20.f, output);
    ASSERT_EQ(output.ranges[0].count, count);
    ASSERT_EQ(output.ranges[1].count, count);
    EXPECT_TRUE(memory.owns(output.entities.data()));
    EXPECT_TRUE(memory.owns(output.distances.data()));
    EXPECT_TRUE(memory.owns(output.directions.get_const_view().xs().data()));
    EXPECT_TRUE(memory.owns(output.ranges.data()));
}

TEST_F(BulkRangeQueries, InclusiveNegativeAndZeroRadiusPreserveDirectionSemantics) {
    owners.spawn(EntityType::Fighter, {}, {}, 100, Team::Red);
    owners.spawn(EntityType::Fighter, {{0.00001f, 0.f, 0.f}}, {}, 100, Team::Red);
    owners.spawn(EntityType::Fighter, {{3.f, 4.f, 0.f}}, {}, 100, Team::Red);
    owners.spawn(EntityType::Fighter, {{5.01f, 0.f, 0.f}}, {}, 100, Team::Red);
    rebuild();
    request({}, Team::Blue);
    FrameRangeQueryResults output{&memory};
    expect_scalar_matches(-5.f, output);
    EXPECT_EQ(output.ranges[0].count, 3u);
    FrameRangeQueryResults zero_radius_output{&memory};
    expect_scalar_matches(0.f, zero_radius_output);
    EXPECT_EQ(zero_radius_output.ranges[0].count, 1u);
}

TEST_F(BulkRangeQueries, EmptyRequestsAndEmptyResultsUseIndependentOutput) {
    auto const id{owners.spawn(EntityType::Fighter, {}, {}, 100, Team::Red)};
    rebuild();
    request({}, Team::Blue);
    FrameRangeQueryResults output{&memory};
    expect_scalar_matches(10.f, output);
    ASSERT_EQ(output.entities.num(), 1u);
    EXPECT_EQ(output.entities[0], id);

    teams[0] = Team::Red;
    FrameRangeQueryResults same_team_output{&memory};
    expect_scalar_matches(10.f, same_team_output);
    EXPECT_TRUE(same_team_output.entities.is_empty());
    EXPECT_EQ(same_team_output.ranges[0].count, 0u);

    origins.clear();
    teams.clear();
    request({{2000.f, 0.f, 0.f}}, Team::Blue);
    FrameRangeQueryResults outside_grid_output{&memory};
    expect_scalar_matches(10.f, outside_grid_output);
    EXPECT_TRUE(outside_grid_output.entities.is_empty());
    EXPECT_EQ(outside_grid_output.ranges[0].offset, 0u);

    origins.clear();
    teams.clear();
    FrameRangeQueryResults empty_output{&memory};
    query(10.f, empty_output);
    EXPECT_TRUE(empty_output.ranges.is_empty());
    EXPECT_TRUE(empty_output.entities.is_empty());
    EXPECT_TRUE(empty_output.distances.is_empty());
    EXPECT_TRUE(empty_output.directions.is_empty());

    // Preserve earlier results while subsequent queries use the same scratch epoch.
    ASSERT_EQ(output.entities.num(), 1u);
    EXPECT_EQ(output.entities[0], id);
}

TEST_F(BulkRangeQueries, EmptyGridHasOneEmptyRangePerRequest) {
    rebuild();
    request({}, Team::Blue);
    request({{200.f, 0.f, 0.f}}, Team::Red);
    FrameRangeQueryResults output{&memory};
    expect_scalar_matches(10.f, output);
    EXPECT_TRUE(output.entities.is_empty());
}
} // namespace ioj::sim::tests
