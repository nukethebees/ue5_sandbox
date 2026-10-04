#include "support/collision_agent_storage.h"
#include <ioj/sim/spatial_query_manager.h>

#include <sandbox/core/frame_memory_resource.h>

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

struct FrameBacking {
    alignas(ml::FrameMemoryResource::backing_alignment) std::array<std::byte, 256 * 1024> bytes{};
    ml::FrameMemoryResource resource{bytes};
};

void initialise_queries(SpatialQueryManager& manager, CollisionAgentStorage const& owners) {
    collision::EntityAABBs bounds;
    for (auto const type : ml::EnumTraits<EntityType>::values) {
        bounds.set_centre(type, {});
        bounds.set_half_extents(type, Vector3f{{2.f, 2.f, 2.f}});
    }
    manager.initialise({{10, 10, 10}, {{10.f, 10.f, 10.f}}}, bounds);
    owners.refresh(manager);
}
}

namespace ioj::sim::tests {
TEST(QueryScratch, ExhaustionDoesNotFallBackToHeap) {
    CollisionAgentStorage owners;
    owners.publish();
    SpatialQueryManager manager{owners.entity_tables};
    query_scratch_detail::initialise_queries(manager, owners);
    alignas(ml::FrameMemoryResource::backing_alignment) std::array<std::byte, 64> backing{};
    ml::FrameMemoryResource frame{backing};
    {
        ml::FrameScratchScope scope{frame};
        EXPECT_THROW(manager.trace_closest({}, {}, &frame), std::bad_alloc);
        EXPECT_EQ(frame.get_stats().overflow_count, 1u);
        EXPECT_EQ(frame.get_stats().outstanding_allocation_count, 0u);
    }
}

TEST(QueryScratch, ConcurrentQueriesAllocateFromSharedFrameAndReleaseBeforeReclaim) {
    CollisionAgentStorage owners;
    auto const id{owners.spawn(EntityType::CapitalShip)};
    owners.publish();
    SpatialQueryManager manager{owners.entity_tables};
    query_scratch_detail::initialise_queries(manager, owners);
    query_scratch_detail::FrameBacking frame;
    {
        ml::FrameScratchScope scope{frame.resource};
        std::barrier start{4};
        std::array<std::jthread, 4> workers;
        for (auto& worker : workers) {
            worker = std::jthread{[&] {
                start.arrive_and_wait();
                auto const hit{manager.trace_closest(
                    {{-10.f, 0.f, 0.f}}, {{10.f, 0.f, 0.f}}, &frame.resource)};
                EXPECT_EQ(hit.entity, id);
                std::array<EntityUniqueId, 4> nearby;
                EXPECT_EQ(manager.collect_non_team_entities_in_range(
                              {}, Team::Green, 20.f, nearby, &frame.resource),
                          1u);
                EXPECT_EQ(nearby[0], id);
            }};
        }
        for (auto& worker : workers) {
            worker.join();
        }
        EXPECT_GT(frame.resource.get_stats().current_claimed_bytes, 0u);
        EXPECT_EQ(frame.resource.get_stats().outstanding_allocation_count, 0u);
    }
    EXPECT_EQ(frame.resource.get_stats().current_claimed_bytes, 0u);
    EXPECT_EQ(frame.resource.get_stats().overflow_count, 0u);
}

TEST(QueryScratch, ManagerUsesExplicitScratchAndReleasesQueriesWithinEachEpoch) {
    CollisionAgentStorage owners;
    auto const id{owners.spawn(EntityType::CapitalShip)};
    owners.publish();
    query_scratch_detail::CountingResource persistent;
    SpatialQueryManager manager{owners.entity_tables, &persistent};
    query_scratch_detail::initialise_queries(manager, owners);
    query_scratch_detail::FrameBacking frame;
    for (int epoch{}; epoch < 3; ++epoch) {
        {
            ml::FrameScratchScope scratch_scope{frame.resource};
            auto const persistent_allocations{persistent.allocations.load()};
            auto const hit{
                manager.trace_closest({{-10.f, 0.f, 0.f}}, {{10.f, 0.f, 0.f}}, &frame.resource)};
            EXPECT_EQ(hit.entity, id);
            std::array<EntityUniqueId, 4> nearby;
            ASSERT_EQ(manager.collect_non_team_entities_in_range(
                          {}, Team::Green, 20.f, nearby, &frame.resource),
                      1u);
            EXPECT_EQ(nearby[0], id);
            EXPECT_GT(frame.resource.get_stats().current_claimed_bytes, 0u);
            EXPECT_EQ(frame.resource.get_stats().outstanding_allocation_count, 0u);
            EXPECT_EQ(persistent.allocations.load(), persistent_allocations);
        }
        EXPECT_EQ(frame.resource.get_stats().outstanding_allocation_count, 0u);
        EXPECT_EQ(frame.resource.get_stats().current_claimed_bytes, 0u);
    }
}

TEST(QueryScratch, PublishedOverlapBatchesSurviveScratchEpochs) {
    CollisionAgentStorage owners;
    std::array const ids{owners.spawn(EntityType::CapitalShip), owners.spawn(EntityType::Fighter)};
    owners.publish();
    query_scratch_detail::CountingResource persistent;
    SpatialQueryManager manager{owners.entity_tables, &persistent};
    query_scratch_detail::initialise_queries(manager, owners);
    query_scratch_detail::FrameBacking frame;
    // Resolve each epoch's outputs and each distinct batch.
    // NOLINTBEGIN(ioj-loop-view-accessor-call)
    for (int epoch{}; epoch < 3; ++epoch) {
        {
            ml::FrameScratchScope scratch_scope{frame.resource};
            auto const overlaps{manager.detect_overlaps(ids, &frame.resource)};
            ASSERT_EQ(overlaps.entity_entity_overlaps.num(), 1u);
            EXPECT_FALSE(
                frame.resource.owns(overlaps.entity_entity_overlaps.first_entities().data()));
        }
        EXPECT_EQ(frame.resource.get_stats().outstanding_allocation_count, 0u);
        EXPECT_EQ(frame.resource.get_stats().current_claimed_bytes, 0u);
        auto const events{manager.get_aabb_overlap_events()};
        ASSERT_EQ(events.batches.size(), static_cast<std::size_t>(epoch + 1));
        for (int batch{}; batch <= epoch; ++batch) {
            auto const overlaps{events.get_batch(batch).overlaps.entity_entity_overlaps};
            ASSERT_EQ(overlaps.num(), 1u);
            EXPECT_EQ(overlaps.first_entities()[0], std::min(ids[0], ids[1]));
            EXPECT_EQ(overlaps.second_entities()[0], std::max(ids[0], ids[1]));
        }
    }
    // NOLINTEND(ioj-loop-view-accessor-call)
    manager.reset_frame_collision_events();
    EXPECT_TRUE(manager.get_aabb_overlap_events().batches.empty());
    EXPECT_GT(persistent.allocations.load(), 0u);
}
}
