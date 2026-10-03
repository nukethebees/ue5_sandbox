#include <ioj/sim/agent_accessor.h>
#include <ioj/sim/column_math.h>
#include <ioj/sim/frame_range_query_results.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/spatial_query_manager.h>
#include <ioj/sim/team_checks.h>

#include <sandbox/core/parallel_for.h>

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/task_arena.h>

#include <cassert>
#include <cmath>
#include <limits>
#include <memory_resource>
#include <vector>

namespace ioj::sim::range_query {
using EntityCount = std::uint32_t;
using RequestIndex = std::uint32_t;
using RequestCount = std::uint32_t;
using WorkerIndex = ml::ThreadIndex;
using WorkerCount = std::uint32_t;
using MatchIndex = std::uint32_t;
using MatchCount = std::uint32_t;

struct Match {
    EntityUniqueId entity;
    float distance{};
    Vector3f direction;
};

struct WorkerScratch {
    explicit WorkerScratch(ml::FrameScratchResource& scratch_resource,
                           EntityCount const entity_count)
        : query_buffers{&scratch_resource}
        , matches{&scratch_resource} {
        query_buffers.ensure_entity_stamp_count(entity_count);
    }

    QueryThreadBuffers query_buffers;
    std::pmr::vector<Match> matches;
};

struct MatchSource {
    WorkerIndex worker_index{};
    MatchIndex offset{};
};

void collect_request_matches(collision::CollisionUniformGrid const& grid,
                             AgentAccessor const& agents,
                             EntityTypeSizes const& entity_offsets,
                             collision::CellCoord const max_grid_coord,
                             Vector3f const radius_extent,
                             float const radius_squared,
                             Vector3f const origin,
                             Team const excluded_team,
                             WorkerScratch& worker) {
    SANDBOX_PROFILE_SCOPE("range_query::collect_request_matches");

    assert(std::isfinite(origin.X) && std::isfinite(origin.Y) && std::isfinite(origin.Z));

    // Restrict the scan's bounding box to the grid.
    auto [min_coord,
          max_coord]{grid.to_cell_coord_bounds(origin - radius_extent, origin + radius_extent)};
    if (max_coord.x < 0 || max_coord.y < 0 || max_coord.z < 0 || min_coord.x > max_grid_coord.x ||
        min_coord.y > max_grid_coord.y || min_coord.z > max_grid_coord.z) {
        return;
    }

    min_coord = min_coord.component_max({});
    max_coord = max_coord.component_min(max_grid_coord);

    // Start a fresh deduplication pass without clearing the stamp buffer.
    auto& stamps{worker.query_buffers.range_query_entity_stamps};
    auto const stamp{worker.query_buffers.advance_range_query_stamp()};
    auto const& indices{agents.indexes()};

    // Rely on the Thinking phase invariants for live membership and valid entity indices.
    for (auto x{min_coord.x}; x <= max_coord.x; ++x) {
        for (auto y{min_coord.y}; y <= max_coord.y; ++y) {
            for (auto z{min_coord.z}; z <= max_coord.z; ++z) {
                // NOLINTNEXTLINE(ioj-loop-view-accessor-call) -- each cell selects different
                // members.
                for (auto const id : grid.get_cell_entities({x, y, z})) {
                    auto const local_index{indices.find(id)};

                    // Visit each entity once, even if it occupies several cells.
                    auto const entity_index{entity_offsets[id.entity_type()] + local_index};
                    if (stamps[entity_index] == stamp) {
                        continue;
                    }

                    stamps[entity_index] = stamp;

                    // Exclude teammates before calculating distance.
                    auto const state{agents.read_spatial(id)};
                    if (state->team == excluded_team) {
                        continue;
                    }

                    // Reject bounding-box candidates outside the scan sphere.
                    auto const delta{state->location - origin};
                    auto const distance_squared{HMM_LenSqrV3(delta)};
                    if (distance_squared > radius_squared) {
                        continue;
                    }

                    // Store the match with a zero direction for coincident positions.
                    auto const distance{std::sqrt(distance_squared)};
                    auto const direction{distance_squared < 1.e-8f ? Vector3f{}
                                                                   : delta * (1.f / distance)};
                    worker.matches.push_back({id, distance, direction});
                }
            }
        }
    }
}
} // namespace ioj::sim::range_query

namespace ioj::sim {
namespace tbb = oneapi::tbb;

void SpatialQueryManager::collect_non_team_entities_in_range(
    Vectors3fConstView const origins,
    std::span<Team const> const teams,
    float const radius,
    FrameRangeQueryResults& out_results,
    ml::FrameScratchResource& scratch_resource) const {
    using namespace range_query;

    SANDBOX_PROFILE_SCOPE("SpatialQueryManager::collect_non_team_entities_in_range_batch");
    assert(origins.num() == teams.size());
    assert(std::isfinite(radius));
    assert(check_valid_teams(teams));

    assert(!out_results.query_started_);
    out_results.query_started_ = true;

    auto const request_count{origins.num()};
    out_results.ranges.set_num(request_count);
    if (request_count == 0) {
        return;
    }

    constexpr RequestCount grain_size{64};
    auto const concurrency{ml::task_arena_concurrency(request_count, grain_size)};
    tbb::task_arena arena{concurrency};
    ml::FrameArray<WorkerScratch> workers{&scratch_resource};
    ml::FrameArray<MatchSource> match_sources{&scratch_resource};
    EntityTypeSizes entity_offsets;

    {
        SANDBOX_PROFILE_SCOPE("prepare workers");

        // Map each entity type into a shared stamp index space.
        auto const counts{agents_.entity_counts()};
        EntityCount entity_count{};
        for (auto const type : ml::EnumTraits<EntityType>::values) {
            entity_offsets[type] = entity_count;
            entity_count += counts[type];
        }

        // Give each worker independent deduplication and match storage.
        {
            SANDBOX_PROFILE_SCOPE("init arena");
            arena.initialize();
        }

        auto const worker_count{static_cast<WorkerCount>(arena.max_concurrency())};
        workers.reserve(worker_count);
        {
            SANDBOX_PROFILE_SCOPE("emplace workers");
            for (WorkerIndex index{}; index < worker_count; ++index) {
                workers.emplace(scratch_resource, entity_count);
            }
        }

        match_sources.set_num(request_count);
    }

    auto const work{tbb::blocked_range<RequestIndex>{0, request_count, grain_size}};
    {
        SANDBOX_PROFILE_SCOPE("scan_requests");
        auto const& grid{collision_system_.uniform_grid_};
        assert(grid.is_configured());
        auto const max_grid_coord{grid.get_max_grid_coord()};
        auto const absolute_radius{std::abs(radius)};
        auto const radius_extent{
            ml::make_vector3f(absolute_radius, absolute_radius, absolute_radius)};
        auto const radius_squared{radius * radius};

        // Scan each request once and retain matches in its worker's buffer.
        arena.execute([&] {
            tbb::parallel_for(work, [&](auto const& chunk) {
                SANDBOX_PROFILE_SCOPE("scan_chunk");
                auto const worker_index{ml::current_thread_index()};
                auto& worker{workers[worker_index]};
                auto const end{chunk.end()};

                for (auto index{chunk.begin()}; index < end; ++index) {
                    auto const size_before_scan{worker.matches.size()};
                    assert(std::in_range<MatchIndex>(size_before_scan));
                    auto const first{static_cast<MatchIndex>(size_before_scan)};

                    collect_request_matches(grid,
                                            agents_,
                                            entity_offsets,
                                            max_grid_coord,
                                            radius_extent,
                                            radius_squared,
                                            origins[index],
                                            teams[index],
                                            worker);

                    auto const size_after_scan{worker.matches.size()};
                    assert(std::in_range<MatchCount>(size_after_scan));

                    // Record the request's slice for later publication.
                    match_sources[index] = {worker_index, first};
                    out_results.ranges[index].count =
                        static_cast<MatchCount>(size_after_scan) - first;
                }
            });
        });
    }

    {
        SANDBOX_PROFILE_SCOPE("publish_matches");
        // Assign disjoint output slices in request order.
        MatchCount match_count{};
        for (auto& range : out_results.ranges) {
            range.offset = match_count;
            assert(range.count <= std::numeric_limits<MatchCount>::max() - match_count);
            match_count += range.count;
        }

        out_results.set_num_matches(match_count);
        auto const directions{out_results.directions.get_view()};

        // Copy worker matches into the caller's contiguous result arrays.
        arena.execute([&] {
            tbb::parallel_for(work, [&](auto const& chunk) {
                auto const end{chunk.end()};
                for (auto index{chunk.begin()}; index < end; ++index) {
                    auto const source{match_sources[index]};
                    auto const range{out_results.ranges[index]};
                    auto const& matches{workers[source.worker_index].matches};
                    auto const count{range.count};

                    for (MatchIndex match_index{}; match_index < count; ++match_index) {
                        auto const& match{matches[source.offset + match_index]};
                        auto const output_index{range.offset + match_index};
                        out_results.entities[output_index] = match.entity;
                        out_results.distances[output_index] = match.distance;
                        set_vector(directions, output_index, match.direction);
                    }
                }
            });
        });
    }
}
} // namespace ioj::sim
