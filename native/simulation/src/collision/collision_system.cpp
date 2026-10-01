#include "ioj/sim/collision/collision_system.h"

#include <ioj/sim/agent_accessor.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/rotator_math.h>

#include <sandbox/core/frame_array.h>
#include <sandbox/core/frame_memory_resource.h>

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <utility>

namespace ioj::sim::collision {
/* **************************************** */
// Construction and setup
/* **************************************** */
CollisionSystem::CollisionSystem(AgentAccessor const& agents) noexcept
    : agents_{agents}
    , uniform_grid_{agents} {}
void CollisionSystem::initialise(GridGeometry const grid_geometry,
                                 collision::EntityAABBs const& bounds) {
    uniform_grid_.set_geometry(grid_geometry);
    entity_aabbs_ = bounds;
    entity_entity_overlaps_.reset();
    entity_static_overlaps_.reset();
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
void CollisionSystem::refresh_spatial_index() {
    SANDBOX_PROFILE_SCOPE("CollisionSystem::refresh_spatial_index");
    uniform_grid_.rebuild_entity_grid(entity_aabbs_);
}

/* **************************************** */
// Overlap detection
/* **************************************** */
auto CollisionSystem::detect_overlaps(std::span<EntityUniqueId const> const overlap_candidates,
                                      ml::FrameScratch& scratch) -> DetectedOverlapsView {
    SANDBOX_PROFILE_SCOPE("CollisionSystem::detect_overlaps");
    collect_overlaps_for_candidates(overlap_candidates, scratch);

    auto const entity_entity_overlaps{entity_entity_overlaps_.get_const_view()};
    auto const entity_static_overlaps{entity_static_overlaps_.get_const_view()};
    overlap_event_storage_.append_batch(entity_entity_overlaps, entity_static_overlaps);

    return {entity_entity_overlaps, entity_static_overlaps};
}
void CollisionSystem::collect_overlaps_for_candidates(
    std::span<EntityUniqueId const> const overlap_candidates, ml::FrameScratch& scratch) {
    SANDBOX_PROFILE_SCOPE("CollisionSystem::collect_overlaps_for_candidates");

    entity_entity_overlaps_.reset();
    entity_static_overlaps_.reset();

    if (overlap_candidates.empty()) {
        return;
    }

    assert(std::in_range<std::uint32_t>(overlap_candidates.size()));
    auto const candidate_count{static_cast<std::uint32_t>(overlap_candidates.size())};
    ml::FrameArray<WorldAABB> bounds{&scratch};
    ml::FrameArray<CollisionUniformGrid::OverlapCounts> counts{&scratch};
    ml::FrameArray<CollisionUniformGrid::OverlapCounts> offsets{&scratch};
    bounds.set_num(candidate_count);
    counts.set_num(candidate_count);
    offsets.set_num(candidate_count);

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
                auto const state{agents_.read_alive(id)};
                if (!state) {
                    continue;
                }
                bounds[index] = collision::make_entity_world_bounds(entity_aabbs_,
                                                                    id.entity_type(),
                                                                    state->location,
                                                                    to_quaternion(state->rotation));
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
        entity_entity_overlaps_.set_num(static_cast<std::uint32_t>(entity_total));
        entity_static_overlaps_.set_num(static_cast<std::uint32_t>(static_total));
    }

    auto const entity_output{entity_entity_overlaps_.get_view()};
    auto const static_output{entity_static_overlaps_.get_view()};
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
                    entity_output.first_entities.subspan(offset.entities, count.entities)};
                auto const second_entities{
                    entity_output.second_entities.subspan(offset.entities, count.entities)};
                auto const static_entities{
                    static_output.entities.subspan(offset.static_geometry, count.static_geometry)};
                auto const static_indices{static_output.static_geometry_indices.subspan(
                    offset.static_geometry, count.static_geometry)};

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

    finalize_overlaps(scratch);
}
void CollisionSystem::finalize_overlaps(ml::FrameScratch& scratch) {
    SANDBOX_PROFILE_SCOPE("CollisionSystem::finalize_overlaps");
    auto const entity_overlap_count{entity_entity_overlaps_.num()};
    auto const static_overlap_count{entity_static_overlaps_.num()};
    auto const sort_index_count{std::max(entity_overlap_count, static_overlap_count)};
    ml::FrameArray<std::int32_t> sort_indices{&scratch};
    sort_indices.reserve(sort_index_count);

    if (entity_overlap_count > 1) {
        sort_indices.set_num(entity_overlap_count);
        entity_entity_overlaps_.sort(
            [](EntityEntityOverlaps const& values, std::int32_t const lhs, std::int32_t const rhs) {
                auto const lhs_first{values.first_entities[lhs]};
                auto const rhs_first{values.first_entities[rhs]};
                return lhs_first < rhs_first ||
                       (lhs_first == rhs_first &&
                        values.second_entities[lhs] < values.second_entities[rhs]);
            },
            sort_indices.view());

        std::uint32_t write_index{1};
        for (std::uint32_t read_index{1}; read_index < entity_overlap_count; ++read_index) {
            if (entity_entity_overlaps_.first_entities[read_index] ==
                    entity_entity_overlaps_.first_entities[write_index - 1] &&
                entity_entity_overlaps_.second_entities[read_index] ==
                    entity_entity_overlaps_.second_entities[write_index - 1]) {
                continue;
            }
            entity_entity_overlaps_.set(write_index,
                                        entity_entity_overlaps_.first_entities[read_index],
                                        entity_entity_overlaps_.second_entities[read_index]);
            ++write_index;
        }
        entity_entity_overlaps_.set_num(write_index);
    }

    if (static_overlap_count > 1) {
        sort_indices.set_num(static_overlap_count);
        entity_static_overlaps_.sort(
            [](EntityStaticOverlaps const& values, std::int32_t const lhs, std::int32_t const rhs) {
                auto const lhs_entity{values.entities[lhs]};
                auto const rhs_entity{values.entities[rhs]};
                return lhs_entity < rhs_entity ||
                       (lhs_entity == rhs_entity &&
                        values.static_geometry_indices[lhs] < values.static_geometry_indices[rhs]);
            },
            sort_indices.view());

        std::uint32_t write_index{1};
        for (std::uint32_t read_index{1}; read_index < static_overlap_count; ++read_index) {
            if (entity_static_overlaps_.entities[read_index] ==
                    entity_static_overlaps_.entities[write_index - 1] &&
                entity_static_overlaps_.static_geometry_indices[read_index] ==
                    entity_static_overlaps_.static_geometry_indices[write_index - 1]) {
                continue;
            }
            entity_static_overlaps_.set(
                write_index,
                entity_static_overlaps_.entities[read_index],
                entity_static_overlaps_.static_geometry_indices[read_index]);
            ++write_index;
        }
        entity_static_overlaps_.set_num(write_index);
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
