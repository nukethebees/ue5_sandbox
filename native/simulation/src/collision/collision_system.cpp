#include "ioj/sim/collision/collision_system.h"

#include <ioj/sim/entity_cell_data_operations.h>
#include <ioj/sim/entity_tables.h>
#include <ioj/sim/profiling.h>

#include <sandbox/core/frame_array.h>
#include <sandbox/core/frame_memory_resource.h>
#include <sandbox/core/soa_permutation.h>

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <numeric>
#include <utility>

namespace ioj::sim::collision {
/* **************************************** */
// Construction and setup
/* **************************************** */
CollisionSystem::CollisionSystem(EntityTables const& agents,
                                 std::pmr::memory_resource* resource) noexcept
    : entity_tables_{agents}
    , overlap_event_storage_{resource} {}
void CollisionSystem::initialise(GridGeometry const grid_geometry,
                                 collision::EntityAABBs const& bounds) {
    uniform_grid_.set_geometry(grid_geometry);
    entity_aabbs_ = bounds;
    reset_frame_collision_events();
}
void CollisionSystem::set_static_collision(WorldAABBs bounds) {
    uniform_grid_.set_static_aabbs(std::move(bounds));
}
auto CollisionSystem::add_static_collision_aabb(Vector3f const min_point, Vector3f const max_point)
    -> StaticGeometryIndex {
    return uniform_grid_.add_static_aabb(min_point, max_point);
}

/* **************************************** */
// Spatial-index lifecycle
/* **************************************** */
void CollisionSystem::refresh_spatial_index(LevelReadAccess const& level) {
    SANDBOX_PROFILE_SCOPE("CollisionSystem::refresh_spatial_index");
    uniform_grid_.rebuild_entity_grid(entity_aabbs_, level);
}

/* **************************************** */
// Overlap detection
/* **************************************** */
auto CollisionSystem::detect_overlaps(std::span<EntityUniqueId const> const overlap_candidates,
                                      ml::FrameMemoryResource* const scratch_resource)
    -> DetectedOverlapsView {
    SANDBOX_PROFILE_SCOPE("CollisionSystem::detect_overlaps");
    EntityEntityOverlaps entity_overlaps{scratch_resource};
    EntityStaticOverlaps static_overlaps{scratch_resource};
    collect_overlaps_for_candidates(
        overlap_candidates, entity_overlaps, static_overlaps, scratch_resource);

    auto const entity_entity_overlaps{entity_overlaps.get_const_view()};
    auto const entity_static_overlaps{static_overlaps.get_const_view()};
    overlap_event_storage_.append_batch(entity_entity_overlaps, entity_static_overlaps);

    auto const events{overlap_event_storage_.get_view()};
    return events.get_batch(static_cast<AABBOverlapEventBatchIndex>(events.batches.size() - 1))
        .overlaps;
}
void CollisionSystem::collect_overlaps_for_candidates(
    std::span<EntityUniqueId const> const overlap_candidates,
    EntityEntityOverlaps& entity_overlaps,
    EntityStaticOverlaps& static_overlaps,
    ml::FrameMemoryResource* const scratch_resource) {
    SANDBOX_PROFILE_SCOPE("CollisionSystem::collect_overlaps_for_candidates");

    if (overlap_candidates.empty()) {
        return;
    }

    assert(std::in_range<std::uint32_t>(overlap_candidates.size()));
    auto const candidate_count{static_cast<std::uint32_t>(overlap_candidates.size())};
    ml::FrameArray<WorldAABB> bounds{scratch_resource};
    ml::FrameArray<CollisionUniformGrid::OverlapCounts> counts{scratch_resource};
    ml::FrameArray<CollisionUniformGrid::OverlapCounts> offsets{scratch_resource};
    bounds.set_num(candidate_count);
    counts.set_num(candidate_count);
    offsets.set_num(candidate_count);

    ml::FrameArray<std::uint32_t> order{scratch_resource};
    ml::FrameArray<EntityInstanceHandle> handles{scratch_resource};
    ml::FrameArray<std::uint8_t> present{scratch_resource};
    order.set_num(candidate_count);
    handles.set_num(candidate_count);
    present.set_num(candidate_count);
    auto const runs{entity_tables_.lookups.lookup_handles(overlap_candidates, order, handles)};
    auto const built{uniform_grid_.get_entity_world_bounds()};
    auto const built_ids{built.entity_ids()};
    for (std::uint32_t run{}; run < runs.num; ++run) {
        // Bind the row mapping once for each populated entity-type run.
        // NOLINTNEXTLINE(ioj-loop-view-accessor-call)
        auto const rows{uniform_grid_.entity_row_to_aabb_row(runs.types[run])};
        auto const end{runs.end(run)};
        for (auto index{runs.offsets[run]}; index < end; ++index) {
            auto const candidate{order[index]};
            auto const handle{handles[candidate]};
            if (!handle.is_valid() || handle.index() >= rows.size()) {
                continue;
            }
            auto const row{rows[handle.index()]};
            if (row == EntityInstanceHandle::invalid_value ||
                built_ids[row] != overlap_candidates[candidate]) {
                continue;
            }
            bounds[candidate] = {min_point_at(built, row), max_point_at(built, row)};
            present[candidate] = 1;
        }
    }

    // Count first so each candidate can write into a disjoint output span without allocating.
    constexpr std::uint32_t grain_size{64};
    auto const range{oneapi::tbb::blocked_range<std::uint32_t>{0, candidate_count, grain_size}};
    {
        SANDBOX_PROFILE_SCOPE("overlap_count");
        oneapi::tbb::parallel_for(range, [&](auto const& chunk) {
            SANDBOX_PROFILE_SCOPE("overlap_count_chunk");
            auto const end{chunk.end()};
            for (auto index{chunk.begin()}; index < end; ++index) {
                auto const id{overlap_candidates[index]};
                if (!present[index]) {
                    continue;
                }
                counts[index] = uniform_grid_.count_overlaps(bounds[index], id);
            }
        });
    }

    {
        SANDBOX_PROFILE_SCOPE("overlap_output_setup");
        std::uint64_t entity_total{};
        std::uint64_t static_total{};
        for (std::uint32_t index{}; index < candidate_count; ++index) {
            offsets[index] = {static_cast<std::uint32_t>(entity_total),
                              static_cast<std::uint32_t>(static_total)};
            entity_total += counts[index].entities;
            static_total += counts[index].static_geometry;
            assert(std::in_range<std::int32_t>(entity_total));
            assert(std::in_range<std::int32_t>(static_total));
        }
        entity_overlaps.set_num(static_cast<std::uint32_t>(entity_total));
        static_overlaps.set_num(static_cast<std::uint32_t>(static_total));
    }

    auto const entity_output{entity_overlaps.get_view()};
    auto const static_output{static_overlaps.get_view()};
    auto const first_entity_output{entity_output.first_entities()};
    auto const second_entity_output{entity_output.second_entities()};
    auto const static_entity_output{static_output.entities()};
    auto const static_index_output{static_output.static_geometry_indices()};
    {
        SANDBOX_PROFILE_SCOPE("overlap_fill");
        oneapi::tbb::parallel_for(range, [&](auto const& chunk) {
            SANDBOX_PROFILE_SCOPE("overlap_fill_chunk");
            auto const end{chunk.end()};
            for (auto index{chunk.begin()}; index < end; ++index) {
                auto const count{counts[index]};
                if (count.entities == 0 && count.static_geometry == 0) {
                    continue;
                }
                auto const id{overlap_candidates[index]};
                auto const offset{offsets[index]};
                auto const first_entities{
                    first_entity_output.subspan(offset.entities, count.entities)};
                auto const second_entities{
                    second_entity_output.subspan(offset.entities, count.entities)};
                auto const static_entities{
                    static_entity_output.subspan(offset.static_geometry, count.static_geometry)};
                auto const static_indices{
                    static_index_output.subspan(offset.static_geometry, count.static_geometry)};

                uniform_grid_.write_overlaps(bounds[index], id, first_entities, static_indices);
                for (std::uint32_t pair{}; pair < count.entities; ++pair) {
                    auto const other{first_entities[pair]};
                    first_entities[pair] = std::min(id, other);
                    second_entities[pair] = std::max(id, other);
                }
                std::ranges::fill(static_entities, id);
            }
        });
    }

    finalize_overlaps(entity_overlaps, static_overlaps, scratch_resource);
}
void CollisionSystem::finalize_overlaps(EntityEntityOverlaps& entity_overlaps,
                                        EntityStaticOverlaps& static_overlaps,
                                        ml::FrameMemoryResource* const scratch_resource) {
    SANDBOX_PROFILE_SCOPE("CollisionSystem::finalize_overlaps");
    auto const entity_overlap_count{entity_overlaps.num()};
    auto const static_overlap_count{static_overlaps.num()};
    auto const sort_index_count{std::max(entity_overlap_count, static_overlap_count)};
    ml::FrameArray<std::int32_t> sort_indices{scratch_resource};
    sort_indices.reserve(sort_index_count);

    auto const entity_columns{entity_overlaps.get_view()};
    auto const static_columns{static_overlaps.get_view()};
    auto const first_entities{entity_columns.first_entities()};
    auto const second_entities{entity_columns.second_entities()};
    auto const static_entities{static_columns.entities()};
    auto const static_indices{static_columns.static_geometry_indices()};

    if (entity_overlap_count > 1) {
        sort_indices.set_num(entity_overlap_count);
        auto const indices{sort_indices.view()};
        std::iota(indices.begin(), indices.end(), 0);
        std::ranges::sort(indices, [&](std::int32_t const lhs, std::int32_t const rhs) {
            auto const lhs_first{first_entities[lhs]};
            auto const rhs_first{first_entities[rhs]};
            return lhs_first < rhs_first ||
                   (lhs_first == rhs_first && second_entities[lhs] < second_entities[rhs]);
        });
        ml::apply_permutation(first_entities, indices);
        ml::apply_permutation(second_entities, indices);

        std::uint32_t write_index{1};
        for (std::uint32_t read_index{1}; read_index < entity_overlap_count; ++read_index) {
            if (first_entities[read_index] == first_entities[write_index - 1] &&
                second_entities[read_index] == second_entities[write_index - 1]) {
                continue;
            }
            entity_overlaps.copy_element(write_index, entity_columns, read_index);
            ++write_index;
        }
        entity_overlaps.set_num(write_index);
    }

    if (static_overlap_count > 1) {
        sort_indices.set_num(static_overlap_count);
        auto const indices{sort_indices.view()};
        std::iota(indices.begin(), indices.end(), 0);
        std::ranges::sort(indices, [&](std::int32_t const lhs, std::int32_t const rhs) {
            auto const lhs_entity{static_entities[lhs]};
            auto const rhs_entity{static_entities[rhs]};
            return lhs_entity < rhs_entity ||
                   (lhs_entity == rhs_entity && static_indices[lhs] < static_indices[rhs]);
        });
        ml::apply_permutation(static_entities, indices);
        ml::apply_permutation(static_indices, indices);

        std::uint32_t write_index{1};
        for (std::uint32_t read_index{1}; read_index < static_overlap_count; ++read_index) {
            if (static_entities[read_index] == static_entities[write_index - 1] &&
                static_indices[read_index] == static_indices[write_index - 1]) {
                continue;
            }
            static_overlaps.copy_element(write_index, static_columns, read_index);
            ++write_index;
        }
        static_overlaps.set_num(write_index);
    }
}

/* **************************************** */
// Events and bounds
/* **************************************** */
void CollisionSystem::reset_frame_collision_events() {
    overlap_event_storage_.reset();
}
auto CollisionSystem::get_entity_collision_bounds() const -> EntityCellData::ConstView {
    return uniform_grid_.get_entity_world_bounds();
}
auto CollisionSystem::get_static_collision_bounds() const -> WorldAABBs::ConstView {
    return uniform_grid_.get_static_aabbs().get_const_view();
}
}
