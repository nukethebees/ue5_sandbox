#include "support/collision_agent_storage.h"
#include "support/simulation_checks.h"
#include "support/simulation_test_support.h"
#include <ioj/bridge/mission_frame.h>
#include <ioj/frame_memory_resource.h>
#include <ioj/memory/bump_resource.h>
#include <ioj/presentation/hud_model.h>
#include <ioj/sim/collision/spatial_query_manager.h>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <barrier>
#include <memory_resource>
#include <thread>

namespace ioj::sim::tests::query_scratch_detail {
class CountingResource final : public std::pmr::memory_resource {
  public:
    std::atomic<std::size_t> allocations{};
    std::atomic<std::size_t> outstanding{};
  private:
    auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override;
    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override;
    auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override {
        return this == &other;
    }
};
auto CountingResource::do_allocate(std::size_t const bytes, std::size_t const alignment) -> void* {
    auto* const result{std::pmr::new_delete_resource()->allocate(bytes, alignment)};
    ++allocations;
    ++outstanding;
    return result;
}
void CountingResource::do_deallocate(void* pointer,
                                     std::size_t const bytes,
                                     std::size_t const alignment) {
    --outstanding;
    std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
}

auto trace_capital(SpatialQueryManager const& queries, FrameMemoryResource& memory) -> EntityId {
    Vectors3f starts{memory};
    Vectors3f ends{memory};
    starts.add({-10.f, 0.f, 0.f});
    ends.add({10.f, 0.f, 0.f});
    TraceHits hits{memory};
    hits.set_num(1);
    queries.trace_closest_lines(
        starts.const_view().resolve(), ends.const_view().resolve(), hits.view());
    return hits.entities()[0];
}

struct FrameBacking {
    FrameMemoryResource resource{256 * 1024};
};

void initialise_queries(SpatialQueryManager& manager, CollisionAgentStorage const& owners) {
    collision::EntityAABBs bounds;
    for (auto const type : EnumTraits<EntityType>::values) {
        bounds.set_centre(type, {});
        bounds.set_half_extents(type, Vector3f{{2.f, 2.f, 2.f}});
    }

    manager.initialise({{10, 10, 10}, {{10.f, 10.f, 10.f}}}, bounds);
    owners.refresh(manager);
}
}

namespace ioj::sim::tests {
TEST(LevelLifetime, DisconnectsHudAndReturnsOverflowBeforeReusingArena) {
    query_scratch_detail::CountingResource upstream;
    auto const fixture{make_fixture()};
    presentation::HudModel hud;
    {
        memory::BumpResource arena{4096, upstream};
        for (unsigned mission{}; mission < 3; ++mission) {
            SimulationScenario data{make_simulation_data(fixture)};
            add_player_spawn(data, make_player_spawn(fixture));
            {
                LevelSim level{std::move(data).compile(), arena};
                hud.initialise(bridge::mission_frame(level));
                ASSERT_TRUE(hud.active());
                EXPECT_GT(upstream.outstanding.load(), 1u);
                hud.deactivate();
            }

            EXPECT_FALSE(hud.active());
            EXPECT_EQ(upstream.outstanding.load(), 1u);
            EXPECT_GT(arena.used_bytes(), 0u);
            arena.reset();
            EXPECT_EQ(arena.used_bytes(), 0u);
        }
    }

    EXPECT_EQ(upstream.outstanding.load(), 0u);
}

TEST(QueryScratch, ExhaustionForwardsToUpstream) {
    CollisionAgentStorage owners;
    owners.publish();
    SpatialQueryManager manager{};
    query_scratch_detail::initialise_queries(manager, owners);
    query_scratch_detail::CountingResource upstream;
    {
        FrameMemoryResource frame{64, upstream};
        FrameScratchScope scope{frame};
        EXPECT_NO_THROW(query_scratch_detail::trace_capital(manager, frame));
        EXPECT_GT(frame.stats().overflow_count, 0u);
        EXPECT_EQ(frame.stats().outstanding_allocation_count, 0u);
        EXPECT_GT(upstream.allocations.load(), 1u);
        EXPECT_EQ(upstream.outstanding.load(), 1u);
    }

    EXPECT_EQ(upstream.outstanding.load(), 0u);
}

TEST(QueryScratch, ConcurrentQueriesAllocateFromSharedFrameAndReleaseBeforeReclaim) {
    CollisionAgentStorage owners;
    auto const id{owners.spawn(EntityType::CapitalShip)};
    owners.publish();
    SpatialQueryManager manager{};
    query_scratch_detail::initialise_queries(manager, owners);
    query_scratch_detail::FrameBacking frame;
    {
        FrameScratchScope scope{frame.resource};
        std::barrier start{4};
        std::array<std::jthread, 4> workers;
        for (auto& worker : workers) {
            worker = std::jthread{[&] {
                start.arrive_and_wait();
                EXPECT_EQ(query_scratch_detail::trace_capital(manager, frame.resource), id);
                std::array<EntityId, 4> nearby;
                EXPECT_EQ(manager.collect_non_team_entities_in_range(owners.entity_tables.lookups,
                                                                     {},
                                                                     Team::Green,
                                                                     20.f,
                                                                     nearby,
                                                                     &frame.resource),
                          1u);
                EXPECT_EQ(nearby[0], id);
            }};
        }

        for (auto& worker : workers) {
            worker.join();
        }

        EXPECT_GT(frame.resource.stats().current_claimed_bytes, 0u);
        EXPECT_EQ(frame.resource.stats().outstanding_allocation_count, 0u);
    }

    EXPECT_EQ(frame.resource.stats().current_claimed_bytes, 0u);
    EXPECT_EQ(frame.resource.stats().overflow_count, 0u);
}

TEST(QueryScratch, ManagerUsesExplicitScratchAndReleasesQueriesWithinEachEpoch) {
    CollisionAgentStorage owners;
    auto const id{owners.spawn(EntityType::CapitalShip)};
    owners.publish();
    SpatialQueryManager manager{};
    query_scratch_detail::initialise_queries(manager, owners);
    query_scratch_detail::FrameBacking frame;
    for (int epoch{}; epoch < 3; ++epoch) {
        {
            FrameScratchScope scratch_scope{frame.resource};
            std::array<EntityId, 4> nearby;
            ASSERT_EQ(
                manager.collect_non_team_entities_in_range(
                    owners.entity_tables.lookups, {}, Team::Green, 20.f, nearby, &frame.resource),
                1u);
            EXPECT_EQ(nearby[0], id);
            EXPECT_GT(frame.resource.stats().current_claimed_bytes, 0u);
            EXPECT_EQ(frame.resource.stats().outstanding_allocation_count, 0u);
        }

        expect_frame_reclaimed(frame.resource.stats());
    }
}

TEST(QueryScratch, OverlapOutputsReleaseBeforeScratchReclaim) {
    CollisionAgentStorage owners;
    std::array const ids{owners.spawn(EntityType::CapitalShip), owners.spawn(EntityType::Fighter)};
    owners.publish();
    SpatialQueryManager manager{};
    query_scratch_detail::initialise_queries(manager, owners);
    query_scratch_detail::FrameBacking frame;
    for (int epoch{}; epoch < 3; ++epoch) {
        {
            FrameScratchScope scope{frame.resource};
            collision::EntityEntityOverlaps entities{frame.resource};
            collision::EntityStaticOverlaps statics{frame.resource};
            manager.detect_overlaps(
                owners.entity_tables.lookups, ids, entities, statics, &frame.resource);
            ASSERT_EQ(entities.num(), 1u);
            EXPECT_TRUE(frame.resource.owns(entities.first_entities().data()));
            EXPECT_EQ(entities.first_entities()[0], std::min(ids[0], ids[1]));
            EXPECT_EQ(entities.second_entities()[0], std::max(ids[0], ids[1]));
        }

        expect_frame_reclaimed(frame.resource.stats());
    }
}
}
