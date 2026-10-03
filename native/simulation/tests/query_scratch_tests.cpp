#include "support/collision_agent_storage.h"
#include <ioj/sim/query_thread_buffer_pool.h>
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

void fill_buffers(QueryThreadBuffers& buffers, std::uint32_t const count) {
    buffers.line_traces.set_num(count);
    buffers.trace_hits.set_num(count);
    buffers.ensure_entity_stamp_count(count);
}

void expect_frame_storage(QueryThreadBuffers const& buffers, ml::FrameMemoryResource const& frame) {
    auto const traces{buffers.line_traces.get_const_view()};
    auto const hits{buffers.trace_hits.get_const_view()};
    traces.each_column([&](auto column) { EXPECT_TRUE(frame.owns(column.data())); });
    hits.each_column([&](auto column) { EXPECT_TRUE(frame.owns(column.data())); });
    EXPECT_TRUE(frame.owns(buffers.range_query_entity_stamps.data()));
}

void initialise_queries(SpatialQueryManager& manager) {
    collision::EntityAABBs bounds;
    for (auto const type : ml::EnumTraits<EntityType>::values) {
        bounds.set_centre(type, {});
        bounds.set_half_extents(type, Vector3f{{2.f, 2.f, 2.f}});
    }
    manager.initialise({{10, 10, 10}, {{10.f, 10.f, 10.f}}}, bounds);
    manager.refresh_spatial_index();
}
}

namespace ioj::sim::tests {
TEST(QueryScratch, ExhaustionDoesNotFallBackToHeap) {
    alignas(ml::FrameMemoryResource::backing_alignment) std::array<std::byte, 64> backing{};
    ml::FrameMemoryResource frame{backing};
    {
        ml::FrameScratchScope scope{frame};
        QueryThreadBuffers buffers{&scope.scratch()};
        EXPECT_THROW(buffers.line_traces.set_num(64), std::bad_alloc);
        EXPECT_TRUE(buffers.line_traces.is_empty());
        EXPECT_EQ(frame.get_stats().overflow_count, 1u);
        EXPECT_EQ(frame.get_stats().outstanding_allocation_count, 0u);
    }
}

TEST(QueryScratch, PoolPropagatesResourcesThroughGrowthAndReleasesStorage) {
    query_scratch_detail::CountingResource bookkeeping;
    query_scratch_detail::CountingResource buffers;
    {
        QueryThreadBufferPool pool{&bookkeeping, &buffers};
        ASSERT_EQ(pool.reserve(1), QueryThreadBufferReserveResult::reserved);
        auto const index{pool.try_acquire()};
        ASSERT_TRUE(index.has_value());
        query_scratch_detail::fill_buffers(pool.get(*index), 4);
        pool.get(*index).trace_hits.get_view().hits()[0] = 1;
        EXPECT_TRUE(pool.release(*index));

        ASSERT_EQ(pool.reserve(4), QueryThreadBufferReserveResult::reserved);
        EXPECT_EQ(pool.get(*index).trace_hits.get_const_view().hits()[0], 1);
        for (std::uint32_t i{}; i < 4; ++i) {
            auto& slot{pool.get(i)};
            EXPECT_EQ(slot.line_traces.get_memory_resource(), &buffers);
            EXPECT_EQ(slot.trace_hits.get_memory_resource(), &buffers);
            EXPECT_EQ(slot.range_query_entity_stamps.get_allocator().resource(), &buffers);
            query_scratch_detail::fill_buffers(slot, 128);
        }
        EXPECT_GT(bookkeeping.allocations.load(), 0u);
        EXPECT_GT(buffers.allocations.load(), 0u);
    }
    EXPECT_EQ(bookkeeping.outstanding.load(), 0u);
    EXPECT_EQ(buffers.outstanding.load(), 0u);
}

TEST(QueryScratch, RejectsResourceChangesWhileLeasedAndReusesScratchWithinScope) {
    QueryThreadBufferPool pool;
    ASSERT_EQ(pool.reserve(1), QueryThreadBufferReserveResult::reserved);
    query_scratch_detail::FrameBacking frame;
    {
        ml::FrameScratchScope scope{frame.resource};
        auto const first{pool.try_acquire()};
        ASSERT_TRUE(first.has_value());
        EXPECT_FALSE(pool.set_buffer_resource(&scope.scratch()));
        EXPECT_EQ(pool.reserve(2), QueryThreadBufferReserveResult::active_queries);
        EXPECT_TRUE(pool.release(*first));

        ASSERT_TRUE(pool.set_buffer_resource(&scope.scratch()));
        auto const index{pool.try_acquire()};
        ASSERT_TRUE(index.has_value());
        query_scratch_detail::fill_buffers(pool.get(*index), 64);
        query_scratch_detail::expect_frame_storage(pool.get(*index), frame.resource);
        auto const claimed{frame.resource.get_stats().current_claimed_bytes};
        EXPECT_TRUE(pool.release(*index));

        auto const reused{pool.try_acquire()};
        ASSERT_TRUE(reused.has_value());
        query_scratch_detail::fill_buffers(pool.get(*reused), 32);
        EXPECT_EQ(frame.resource.get_stats().current_claimed_bytes, claimed);
        EXPECT_TRUE(pool.release(*reused));
        EXPECT_EQ(pool.reserve(3), QueryThreadBufferReserveResult::reserved);
        query_scratch_detail::fill_buffers(pool.get(2), 128);
        query_scratch_detail::expect_frame_storage(pool.get(2), frame.resource);
        EXPECT_TRUE(pool.set_buffer_resource(pool.get_buffer_memory_resource()));
        EXPECT_EQ(frame.resource.get_stats().outstanding_allocation_count, 0u);
    }
    EXPECT_EQ(frame.resource.get_stats().current_claimed_bytes, 0u);
}

TEST(QueryScratch, ConcurrentLeasesAllocateFromSharedFrameAndReleaseBeforeReclaim) {
    QueryThreadBufferPool pool;
    ASSERT_EQ(pool.reserve(4), QueryThreadBufferReserveResult::reserved);
    query_scratch_detail::FrameBacking frame;
    {
        ml::FrameScratchScope scope{frame.resource};
        ASSERT_TRUE(pool.set_buffer_resource(&scope.scratch()));
        std::barrier start{4};
        std::array<std::jthread, 4> workers;
        for (auto& worker : workers) {
            worker = std::jthread{[&] {
                auto const index{pool.try_acquire()};
                start.arrive_and_wait();
                ASSERT_TRUE(index.has_value());
                auto& buffers{pool.get(*index)};
                query_scratch_detail::fill_buffers(buffers, 64);
                query_scratch_detail::fill_buffers(buffers, 257);
                query_scratch_detail::expect_frame_storage(buffers, frame.resource);
                EXPECT_TRUE(pool.release(*index));
            }};
        }
        for (auto& worker : workers) {
            worker.join();
        }
        EXPECT_TRUE(pool.set_buffer_resource(pool.get_buffer_memory_resource()));
        EXPECT_EQ(frame.resource.get_stats().outstanding_allocation_count, 0u);
    }
    EXPECT_EQ(frame.resource.get_stats().current_claimed_bytes, 0u);
    EXPECT_EQ(frame.resource.get_stats().overflow_count, 0u);
}

TEST(QueryScratch, ManagerBindsQueriesToEachEpochAndRestoresOutsideQueries) {
    CollisionAgentStorage owners;
    auto const id{owners.spawn(EntityType::CapitalShip)};
    owners.publish();
    query_scratch_detail::CountingResource bookkeeping;
    query_scratch_detail::CountingResource persistent;
    SpatialQueryManager manager{owners.entity_tables, &bookkeeping, &persistent};
    query_scratch_detail::initialise_queries(manager);
    auto const trace{[&] {
        return manager.trace_closest(Vector3f{{-10.f, 0.f, 0.f}}, Vector3f{{10.f, 0.f, 0.f}});
    }};
    ASSERT_EQ(trace().entity, id);
    EXPECT_GT(persistent.outstanding.load(), 0u);

    query_scratch_detail::FrameBacking frame;
    for (int epoch{}; epoch < 3; ++epoch) {
        {
            ml::FrameScratchScope scratch_scope{frame.resource};
            query_manager::ScratchScope query_scope{manager, scratch_scope.scratch()};
            auto const persistent_allocations{persistent.allocations.load()};
            auto const bookkeeping_allocations{bookkeeping.allocations.load()};
            EXPECT_EQ(persistent.outstanding.load(), 0u);
            EXPECT_EQ(trace().entity, id);
            std::array<EntityUniqueId, 4> nearby;
            ASSERT_EQ(manager.collect_non_team_entities_in_range({}, Team::Green, 20.f, nearby),
                      1u);
            EXPECT_EQ(nearby[0], id);
            EXPECT_GT(frame.resource.get_stats().outstanding_allocation_count, 0u);
            EXPECT_EQ(persistent.allocations.load(), persistent_allocations);
            EXPECT_EQ(bookkeeping.allocations.load(), bookkeeping_allocations);
        }
        EXPECT_EQ(frame.resource.get_stats().outstanding_allocation_count, 0u);
        EXPECT_EQ(frame.resource.get_stats().current_claimed_bytes, 0u);
        EXPECT_EQ(trace().entity, id);
    }
}

TEST(QueryScratch, PublishedOverlapBatchesSurviveScratchEpochs) {
    CollisionAgentStorage owners;
    std::array const ids{owners.spawn(EntityType::CapitalShip), owners.spawn(EntityType::Fighter)};
    owners.publish();
    query_scratch_detail::CountingResource persistent;
    SpatialQueryManager manager{owners.entity_tables, &persistent};
    query_scratch_detail::initialise_queries(manager);
    query_scratch_detail::FrameBacking frame;
    // Resolve each epoch's outputs and each distinct batch.
    // NOLINTBEGIN(ioj-loop-view-accessor-call)
    for (int epoch{}; epoch < 3; ++epoch) {
        {
            ml::FrameScratchScope scratch_scope{frame.resource};
            auto const overlaps{manager.detect_overlaps(ids, scratch_scope.scratch())};
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
