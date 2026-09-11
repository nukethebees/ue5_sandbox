#include "SpaceGameSimulation/simulation/collision_uniform_grid.h"

#include <SandboxCore/soa_rotator_utils.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/entities/TestEntityType.h>
#include <SpaceGameSimulation/simulation/EntityWorldBounds.h>
#include <SpaceGameSimulation/simulation/LineTraces.h>
#include <SpaceGameSimulation/simulation/TraceHits.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace ml::ioj {
namespace {
static_assert(FEntityAABBs::space_ship_index == std::to_underlying(ETestEntityType::PlayerShip));
static_assert(FEntityAABBs::static_turret_index == std::to_underlying(ETestEntityType::Turret));
static_assert(FEntityAABBs::capital_ship_index == std::to_underlying(ETestEntityType::CapitalShip));
static_assert(FEntityAABBs::fighter_index ==
              std::to_underlying(ETestEntityType::CapitalShipFighter));
static_assert(FEntityAABBs::tube_spinner_index == std::to_underlying(ETestEntityType::TubeSpinner));
static_assert(FEntityAABBs::num_rows == std::to_underlying(ETestEntityType::COUNT));

auto to_cell(float const value, float const cell_dim, float const half_grid_extent) -> int32 {
    return FMath::FloorToInt((value + half_grid_extent) / cell_dim);
}

auto to_closed_max_cell(float const value,
                        float const cell_dim,
                        int32 const grid_dim,
                        float const half_grid_extent) -> int32 {
    if (value == half_grid_extent) {
        return grid_dim - 1;
    }

    return to_cell(value, cell_dim, half_grid_extent);
}

auto clip_segment_to_half_open_aabb(FVector3f const start,
                                    FVector3f const end,
                                    FVector3f const bounds_min,
                                    FVector3f const bounds_max_inside,
                                    FVector3f& clipped_start,
                                    FVector3f& clipped_end) -> bool {
    auto const delta{end - start};
    float entry_t{};
    float exit_t{1.f};

    // Intersect the segment's t range with the grid slab on each axis.
    for (int32 axis{}; axis < 3; ++axis) {
        auto const axis_delta{delta[axis]};
        if (axis_delta == 0.f) {
            // A parallel segment overlaps this slab only when its fixed coordinate is inside it.
            if (start[axis] < bounds_min[axis] || start[axis] > bounds_max_inside[axis]) {
                return false;
            }

            continue;
        }

        auto axis_entry_t{(bounds_min[axis] - start[axis]) / axis_delta};
        auto axis_exit_t{(bounds_max_inside[axis] - start[axis]) / axis_delta};
        if (axis_entry_t > axis_exit_t) {
            Swap(axis_entry_t, axis_exit_t);
        }

        entry_t = FMath::Max(entry_t, axis_entry_t);
        exit_t = FMath::Min(exit_t, axis_exit_t);
        if (entry_t > exit_t) {
            return false;
        }
    }

    clipped_start = start + (delta * entry_t);
    clipped_end = start + (delta * exit_t);
    for (int32 axis{}; axis < 3; ++axis) {
        clipped_start[axis] =
            FMath::Clamp(clipped_start[axis], bounds_min[axis], bounds_max_inside[axis]);
        clipped_end[axis] =
            FMath::Clamp(clipped_end[axis], bounds_min[axis], bounds_max_inside[axis]);
    }

    return true;
}
}

auto CollisionUniformGrid::get_grid_dims() const noexcept -> FIntVector3 {
    return grid_dims_;
}
void CollisionUniformGrid::set_grid_dims(FIntVector3 const grid_dims) noexcept {
    grid_dims_ = grid_dims;
}

auto CollisionUniformGrid::get_cell_dims() const noexcept -> FVector3f {
    return cell_dims_;
}
void CollisionUniformGrid::set_cell_dims(FVector3f const cell_dims) noexcept {
    cell_dims_ = cell_dims;
}

CollisionUniformGrid::CollisionUniformGrid(FTestEntityRegistry const& entity_registry) noexcept
    : entity_registry_{entity_registry} {}

auto CollisionUniformGrid::is_configured() const noexcept -> bool {
    if (grid_dims_.X <= 0 || grid_dims_.Y <= 0 || grid_dims_.Z <= 0 || cell_dims_.X <= 0.0f ||
        cell_dims_.Y <= 0.0f || cell_dims_.Z <= 0.0f) {
        return false;
    }

    auto const xy_cell_count{static_cast<int64>(grid_dims_.X) * grid_dims_.Y};
    return xy_cell_count <= (std::numeric_limits<int32>::max() / grid_dims_.Z);
}

auto CollisionUniformGrid::num_cells() const -> int32 {
    return grid_dims_.X * grid_dims_.Y * grid_dims_.Z;
}
auto CollisionUniformGrid::get_cell_entities(FIntVector3 const cell_coord) const
    -> TConstArrayView<FRegistryEntityHandle> {
    checkf(is_cell_coord_in_bounds(cell_coord),
           TEXT("Collision grid cell coordinate %s is outside grid dimensions %s"),
           *to_string(cell_coord),
           *to_string(grid_dims_));

    auto const cell_index{to_index(cell_coord)};
    auto const count{cell_entity_counts_[cell_index]};
    if (count == 0) {
        return {};
    }

    return TConstArrayView<FRegistryEntityHandle>{entities_}.Slice(cell_entity_offsets_[cell_index],
                                                                   count);
}

void CollisionUniformGrid::reset() {
    grid_dims_ = FIntVector3::ZeroValue;
    cell_dims_ = FVector3f::ZeroVector;
    cell_entity_offsets_.Reset();
    cell_entity_counts_.Reset();
    cell_entity_write_indexes_.Reset();
    non_empty_cell_indices_.Reset();
    entities_.Reset();
    aabbs_.reset();
    entities_buffer_.reset();
    static_aabbs_.reset();
    cell_static_range_indices_.Reset();
    static_cell_range_offsets_.Reset();
    static_cell_range_counts_.Reset();
    static_aabb_indices_.Reset();
}

void CollisionUniformGrid::set_static_aabbs(WorldAABBs static_aabbs) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::CollisionUniformGrid::set_static_aabbs);

    if (!is_configured()) {
        UE_LOG(LogSandbox, Fatal, TEXT("Cannot build static geometry for an unconfigured grid"));
    }

    static_aabbs.validate_array_sizes();
    static_aabbs_ = MoveTemp(static_aabbs);
    rebuild_static_grid();
}

auto CollisionUniformGrid::add_static_aabb(FVector3f const min_point, FVector3f const max_point)
    -> int32 {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::CollisionUniformGrid::add_static_aabb);

    checkf(is_configured(), TEXT("Cannot add static geometry to an unconfigured grid"));
    auto const [min_coord, max_coord]{to_cell_coord_bounds(min_point, max_point)};
    checkf(!min_point.ContainsNaN() && !max_point.ContainsNaN() &&
               is_cell_coord_in_bounds(min_coord, max_coord),
           TEXT("Static collision AABB is invalid or outside the collision grid"));

    auto const static_index{static_aabbs_.num()};
    static_aabbs_.mins.add(min_point);
    static_aabbs_.maxes.add(max_point);
    rebuild_static_grid();
    return static_index;
}

void CollisionUniformGrid::rebuild_static_grid() {
    static_cell_range_offsets_.Reset();
    static_cell_range_counts_.Reset();
    static_aabb_indices_.Reset();

    auto const n_cells{num_cells()};
    cell_static_range_indices_.Init(INDEX_NONE, n_cells);

    TArray<int32> cell_counts;
    cell_counts.AddZeroed(n_cells);

    auto const static_count{static_aabbs_.num()};
    for (int32 static_index{}; static_index < static_count; ++static_index) {
        auto const [min_coord, max_coord]{to_cell_coord_bounds(static_aabbs_.mins[static_index],
                                                               static_aabbs_.maxes[static_index])};
        checkf(is_cell_coord_in_bounds(min_coord, max_coord),
               TEXT("Static collision AABB %d is outside the collision grid"),
               static_index);

        for (int32 x{min_coord.X}; x <= max_coord.X; ++x) {
            for (int32 y{min_coord.Y}; y <= max_coord.Y; ++y) {
                for (int32 z{min_coord.Z}; z <= max_coord.Z; ++z) {
                    ++cell_counts[to_index(x, y, z)];
                }
            }
        }
    }

    int64 membership_count{};
    for (int32 cell_index{}; cell_index < n_cells; ++cell_index) {
        auto const count{cell_counts[cell_index]};
        if (count == 0) {
            continue;
        }

        checkf(count <= std::numeric_limits<uint16>::max(),
               TEXT("Static collision cell %d contains %d AABBs, exceeding uint16 capacity"),
               cell_index,
               count);
        checkf(membership_count + count <= std::numeric_limits<int32>::max(),
               TEXT("Static collision grid contains too many cell memberships"));

        cell_static_range_indices_[cell_index] = static_cell_range_offsets_.Num();
        static_cell_range_offsets_.Add(static_cast<uint32>(membership_count));
        static_cell_range_counts_.Add(static_cast<uint16>(count));
        membership_count += count;
    }

    static_aabb_indices_.AddUninitialized(static_cast<int32>(membership_count));
    TArray<uint32> write_indices{static_cell_range_offsets_};

    for (int32 static_index{}; static_index < static_count; ++static_index) {
        auto const [min_coord, max_coord]{to_cell_coord_bounds(static_aabbs_.mins[static_index],
                                                               static_aabbs_.maxes[static_index])};

        for (int32 x{min_coord.X}; x <= max_coord.X; ++x) {
            for (int32 y{min_coord.Y}; y <= max_coord.Y; ++y) {
                for (int32 z{min_coord.Z}; z <= max_coord.Z; ++z) {
                    auto const range_index{cell_static_range_indices_[to_index(x, y, z)]};
                    auto& write_index{write_indices[range_index]};
                    static_aabb_indices_[static_cast<int32>(write_index++)] = static_index;
                }
            }
        }
    }

    auto const range_count{static_cell_range_offsets_.Num()};
    for (int32 range_index{}; range_index < range_count; ++range_index) {
        auto const offset{static_cell_range_offsets_[range_index]};
        auto const count{static_cell_range_counts_[range_index]};
        check(write_indices[range_index] == offset + count);
    }
}

void CollisionUniformGrid::rebuild_grid(FEntityAABBs const& entity_aabbs) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::CollisionUniformGrid::rebuild_grid);

    rebuild_count_.fetch_add(1, std::memory_order_relaxed);

    if (!is_configured()) {
        UE_LOG(LogSandbox,
               Fatal,
               TEXT("Cannot rebuild unconfigured collision grid: cell dimensions are (%g, %g, "
                    "%g), grid dimensions are %s"),
               cell_dims_.X,
               cell_dims_.Y,
               cell_dims_.Z,
               *to_string(grid_dims_));
    }

    auto const& entity_data{entity_registry_.get_entity_data()};
    auto const entity_count{entity_registry_.get_num_elements()};
    auto const gens{entity_registry_.get_generations()};

    auto const n_cells{num_cells()};

    {
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::CollisionUniformGrid::rebuild_grid::prepare_counts);
        for (auto const cell_index : non_empty_cell_indices_) {
            cell_entity_counts_[cell_index] = 0;
        }

        non_empty_cell_indices_.Reset();

        if (cell_entity_counts_.Num() != n_cells) {
            cell_entity_counts_.Reset();
            cell_entity_counts_.AddZeroed(n_cells);
        }

        cell_entity_offsets_.SetNumUninitialized(n_cells);
        cell_entity_write_indexes_.SetNumUninitialized(n_cells);
    }

    entities_buffer_.reset();

    {
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::CollisionUniformGrid::rebuild_grid::count_loop);

        for (int32 i{0}; i < entity_count; ++i) {
            if (entity_data.alive[i] == 0) {
                continue;
            }

            auto const entity_type{entity_data.entity_types[i]};
            auto const aabb_index{std::to_underlying(entity_type)};
            auto const entity_location{entity_data.locations[i]};
            auto const bounds{
                make_entity_world_bounds(entity_aabbs,
                                         aabb_index,
                                         entity_location,
                                         FRotator3f{ml::get_rotator3d(entity_data.rotations, i)})};
            auto const min_point{bounds.Min};
            auto const max_point{bounds.Max};

            auto const [min_coord, max_coord]{to_cell_coord_bounds(min_point, max_point)};

            if (!is_cell_coord_in_bounds(min_coord, max_coord)) {
                FVector3f const grid_dimensions{static_cast<float>(grid_dims_.X),
                                                static_cast<float>(grid_dims_.Y),
                                                static_cast<float>(grid_dims_.Z)};
                auto const half_grid_size{grid_dimensions * cell_dims_ * 0.5f};
                UE_LOG(
                    LogSandbox,
                    Fatal,
                    TEXT("Collision-grid entity %s of type %s has world AABB (%s) through (%s), "
                         "cell AABB %s through %s, outside grid world bounds (%s) through (%s) and "
                         "cell bounds %s through %s"),
                    *LexToString(FRegistryEntityHandle{i, gens[i]}),
                    LexToString(entity_type),
                    *min_point.ToString(),
                    *max_point.ToString(),
                    *to_string(min_coord),
                    *to_string(max_coord),
                    *(-half_grid_size).ToString(),
                    *half_grid_size.ToString(),
                    *to_string(FIntVector3::ZeroValue),
                    *to_string(grid_dims_ - FIntVector3{1, 1, 1}));
            }

            entities_buffer_.min_points.add(min_point);
            entities_buffer_.max_points.add(max_point);
            entities_buffer_.mins.add(min_coord);
            entities_buffer_.maxes.add(max_coord);
            entities_buffer_.handles.Add({i, gens[i]});

            for (int32 x{min_coord.X}; x <= max_coord.X; ++x) {
                for (int32 y{min_coord.Y}; y <= max_coord.Y; ++y) {
                    for (int32 z{min_coord.Z}; z <= max_coord.Z; ++z) {
                        auto const cell_index{to_index(x, y, z)};
                        auto& count{cell_entity_counts_[cell_index]};
                        if (count == 0) {
                            non_empty_cell_indices_.Add(cell_index);
                        }
                        ++count;
                    }
                }
            }
        }
    }

    entities_buffer_.validate_array_sizes();

    auto const n_entries{[&] -> int32 {
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::CollisionUniformGrid::rebuild_grid::count_entries);
        int32 offset{0};

        auto* RESTRICT offsets{cell_entity_offsets_.GetData()};
        auto* RESTRICT write_indexes{cell_entity_write_indexes_.GetData()};
        auto* RESTRICT counts{cell_entity_counts_.GetData()};

        for (auto const cell_index : non_empty_cell_indices_) {
            offsets[cell_index] = offset;
            write_indexes[cell_index] = offset;
            offset += counts[cell_index];
        }
        return offset;
    }()};

    {
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::CollisionUniformGrid::rebuild_grid::prepare_arrays);

        aabbs_.reset();
        aabbs_.add_uninitialised(n_entries);

        entities_.Reset();
        entities_.AddUninitialized(n_entries);
    }

    auto const buffer_count{entities_buffer_.num()};
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::CollisionUniformGrid::rebuild_grid::build_loop);

        for (int32 i{0}; i < buffer_count; ++i) {
            auto const min_coord{entities_buffer_.mins[i]};
            auto const max_coord{entities_buffer_.maxes[i]};

            auto const min_point{entities_buffer_.min_points[i]};
            auto const max_point{entities_buffer_.max_points[i]};

            for (int32 x{min_coord.X}; x <= max_coord.X; ++x) {
                for (int32 y{min_coord.Y}; y <= max_coord.Y; ++y) {
                    for (int32 z{min_coord.Z}; z <= max_coord.Z; ++z) {
                        auto const cell_index{to_index(x, y, z)};
                        auto const write_index{cell_entity_write_indexes_[cell_index]++};

                        entities_[write_index] = entities_buffer_.handles[i];

                        aabbs_.set(write_index, min_point, max_point);
                    }
                }
            }
        }
    }

    {
        TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::CollisionUniformGrid::rebuild_grid::index_check);

        auto* RESTRICT write_indexes{cell_entity_write_indexes_.GetData()};
        auto* RESTRICT offsets{cell_entity_offsets_.GetData()};
        auto* RESTRICT counts{cell_entity_counts_.GetData()};

        for (auto const cell_index : non_empty_cell_indices_) {
            auto const write_index{write_indexes[cell_index]};
            auto const expected{offsets[cell_index] + counts[cell_index]};

            if (write_index != expected) {
                UE_LOG(LogSandbox,
                       Fatal,
                       TEXT("Index incorrect. Got %d, should be %d"),
                       write_index,
                       expected);
            }
        }
    }
}

void CollisionUniformGrid::append_overlaps(FBox3f const& query_bounds,
                                           FRegistryEntityHandle const ignored_entity,
                                           TArray<FRegistryEntityHandle>& out_entities,
                                           TArray<int32>& out_static_geometry_indices) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::CollisionUniformGrid::append_overlaps);

    auto const [min_coord, max_coord]{to_cell_coord_bounds(query_bounds.Min, query_bounds.Max)};
    checkf(is_cell_coord_in_bounds(min_coord, max_coord),
           TEXT("AABB query (%s) through (%s) is outside collision grid dimensions %s"),
           *query_bounds.Min.ToString(),
           *query_bounds.Max.ToString(),
           *to_string(grid_dims_));

    auto const overlaps_query{
        [&query_bounds](FVector3f const candidate_min, FVector3f const candidate_max) {
            return query_bounds.Min.X <= candidate_max.X && query_bounds.Max.X >= candidate_min.X &&
                   query_bounds.Min.Y <= candidate_max.Y && query_bounds.Max.Y >= candidate_min.Y &&
                   query_bounds.Min.Z <= candidate_max.Z && query_bounds.Max.Z >= candidate_min.Z;
        }};
    auto const static_aabbs{static_aabbs_.get_const_view()};
    auto const static_aabb_indices{TConstArrayView<int32>{static_aabb_indices_}};

    for (int32 x{min_coord.X}; x <= max_coord.X; ++x) {
        for (int32 y{min_coord.Y}; y <= max_coord.Y; ++y) {
            for (int32 z{min_coord.Z}; z <= max_coord.Z; ++z) {
                auto const cell_index{to_index(x, y, z)};
                auto const entity_count{cell_entity_counts_[cell_index]};
                if (entity_count > 0) {
                    auto const entity_offset{cell_entity_offsets_[cell_index]};
                    auto const entities{TConstArrayView<FRegistryEntityHandle>{entities_}.Slice(
                        entity_offset, entity_count)};
                    auto const aabbs{aabbs_.get_const_view(entity_offset, entity_count)};

                    for (int32 entity_index{}; entity_index < entity_count; ++entity_index) {
                        auto const entity{entities[entity_index]};
                        if (entity == ignored_entity || !entity_registry_.is_valid_alive(entity)) {
                            continue;
                        }

                        if (overlaps_query(aabbs.mins[entity_index], aabbs.maxes[entity_index])) {
                            out_entities.Add(entity);
                        }
                    }
                }

                auto const static_range_index{cell_static_range_indices_.IsValidIndex(cell_index)
                                                  ? cell_static_range_indices_[cell_index]
                                                  : INDEX_NONE};
                if (static_range_index == INDEX_NONE) {
                    continue;
                }

                auto const static_offset{static_cell_range_offsets_[static_range_index]};
                auto const static_count{static_cell_range_counts_[static_range_index]};
                auto const static_indices{static_aabb_indices.Slice(
                    static_cast<int32>(static_offset), static_cast<int32>(static_count))};
                for (auto const static_index : static_indices) {
                    if (overlaps_query(static_aabbs.mins[static_index],
                                       static_aabbs.maxes[static_index])) {
                        out_static_geometry_indices.Add(static_index);
                    }
                }
            }
        }
    }
}

template <CollisionUniformGrid::ETraceKind TraceKind>
auto CollisionUniformGrid::trace_aabb(WorldAABBs::ConstView const& aabbs,
                                      int32 const aabb_index,
                                      FVector3f const trace_start,
                                      FVector3f const inverse_trace_delta,
                                      FVector3f const trace_delta,
                                      FVector3f const expansion) -> float {
    constexpr auto no_hit{std::numeric_limits<float>::infinity()};

    auto aabb_min{aabbs.mins[aabb_index]};
    auto aabb_max{aabbs.maxes[aabb_index]};
    if constexpr (TraceKind == ETraceKind::Sweep) {
        aabb_min -= expansion;
        aabb_max += expansion;
    } else if constexpr (TraceKind != ETraceKind::Line) {
        static_assert(false, "Unsupported collision trace kind.");
    }

    float tmin{0.f};
    float tmax{1.f};
    for (int32 axis{}; axis < 3; ++axis) {
        auto const slab_min{aabb_min[axis]};
        auto const slab_max{aabb_max[axis]};
        auto const start{trace_start[axis]};
        auto const axis_delta{trace_delta[axis]};

        if (axis_delta == 0.f) {
            if (start < slab_min || start > slab_max) {
                return no_hit;
            }

            continue;
        }

        auto t1{(slab_min - start) * inverse_trace_delta[axis]};
        auto t2{(slab_max - start) * inverse_trace_delta[axis]};
        if (t1 > t2) {
            Swap(t1, t2);
        }

        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) {
            return no_hit;
        }
    }

    return tmin;
}

void CollisionUniformGrid::trace_aabbs(FLineTracesConstView const& traces,
                                       FTraceHitsView const& hits) const {
    line_trace_count_.fetch_add(static_cast<uint64>(traces.num()), std::memory_order_relaxed);
    trace_aabbs_impl<ETraceKind::Line, EIgnoredEntityMode::None, ETraceEntityFilter::None>(
        traces, hits, {}, FVector3f::ZeroVector);
}

void CollisionUniformGrid::trace_aabbs(
    FLineTracesConstView const& traces,
    FTraceHitsView const& hits,
    TConstArrayView<FRegistryEntityHandle> const ignored_entities) const {
    line_trace_count_.fetch_add(static_cast<uint64>(traces.num()), std::memory_order_relaxed);
    trace_aabbs_impl<ETraceKind::Line, EIgnoredEntityMode::PerTrace, ETraceEntityFilter::None>(
        traces, hits, ignored_entities, FVector3f::ZeroVector);
}

void
    CollisionUniformGrid::sweep_aabbs(FLineTracesConstView const& centre_paths,
                                      FVector3f const moving_half_extent,
                                      FTraceHitsView const& hits,
                                      TConstArrayView<FRegistryEntityHandle> const ignored_entities,
                                      ETraceEntityFilter const entity_filter) const {
    sweep_trace_count_.fetch_add(static_cast<uint64>(centre_paths.num()),
                                 std::memory_order_relaxed);
    check(!moving_half_extent.ContainsNaN());
    check(moving_half_extent.X >= 0.f);
    check(moving_half_extent.Y >= 0.f);
    check(moving_half_extent.Z >= 0.f);

    switch (entity_filter) {
        case ETraceEntityFilter::None:
            if (ignored_entities.IsEmpty()) {
                trace_aabbs_impl<ETraceKind::Sweep,
                                 EIgnoredEntityMode::None,
                                 ETraceEntityFilter::None>(
                    centre_paths, hits, ignored_entities, moving_half_extent);
            } else {
                trace_aabbs_impl<ETraceKind::Sweep,
                                 EIgnoredEntityMode::PerTrace,
                                 ETraceEntityFilter::None>(
                    centre_paths, hits, ignored_entities, moving_half_extent);
            }
            return;
        case ETraceEntityFilter::ExcludeCapitalShipFighters:
            if (ignored_entities.IsEmpty()) {
                trace_aabbs_impl<ETraceKind::Sweep,
                                 EIgnoredEntityMode::None,
                                 ETraceEntityFilter::ExcludeCapitalShipFighters>(
                    centre_paths, hits, ignored_entities, moving_half_extent);
            } else {
                trace_aabbs_impl<ETraceKind::Sweep,
                                 EIgnoredEntityMode::PerTrace,
                                 ETraceEntityFilter::ExcludeCapitalShipFighters>(
                    centre_paths, hits, ignored_entities, moving_half_extent);
            }
            return;
    }

    checkNoEntry();
}

void CollisionUniformGrid::reset_runtime_telemetry() noexcept {
    rebuild_count_.store(0, std::memory_order_relaxed);
    line_trace_count_.store(0, std::memory_order_relaxed);
    sweep_trace_count_.store(0, std::memory_order_relaxed);
}

auto CollisionUniformGrid::get_runtime_telemetry() const noexcept
    -> FCollisionGridTelemetrySnapshot {
    return {
        .rebuild_count = rebuild_count_.load(std::memory_order_relaxed),
        .line_trace_count = line_trace_count_.load(std::memory_order_relaxed),
        .sweep_trace_count = sweep_trace_count_.load(std::memory_order_relaxed),
    };
}

template <CollisionUniformGrid::ETraceKind TraceKind,
          CollisionUniformGrid::EIgnoredEntityMode IgnoredEntityMode,
          ETraceEntityFilter EntityFilter>
void CollisionUniformGrid::trace_aabbs_impl(
    FLineTracesConstView const& traces,
    FTraceHitsView const& hits,
    TConstArrayView<FRegistryEntityHandle> const ignored_entities,
    FVector3f const moving_half_extent) const {
    constexpr int32 n_axes{3};

    auto const n{traces.num()};
    check(n == hits.num());
    if constexpr (IgnoredEntityMode == EIgnoredEntityMode::PerTrace) {
        check(ignored_entities.Num() == n);
    } else if constexpr (IgnoredEntityMode != EIgnoredEntityMode::None) {
        static_assert(false, "Unsupported ignored entity mode.");
    }

    auto const grid_width{grid_dims_.X};
    auto const grid_plane_stride{grid_dims_.X * grid_dims_.Y};
    FIntVector3 const max_cell_coord{grid_dims_.X - 1, grid_dims_.Y - 1, grid_dims_.Z - 1};
    FVector3f const grid_dimensions{static_cast<float>(grid_dims_.X),
                                    static_cast<float>(grid_dims_.Y),
                                    static_cast<float>(grid_dims_.Z)};
    auto const half_grid_size{grid_dimensions * cell_dims_ * 0.5f};
    auto const grid_min{-half_grid_size};
    auto const grid_max{half_grid_size};
    auto grid_max_inside{grid_max};
    for (int32 axis{}; axis < n_axes; ++axis) {
        grid_max_inside[axis] = std::nextafter(grid_max[axis], grid_min[axis]);
    }
    auto const to_traversal_cell_coord{[this, max_cell_coord](FVector3f const position) {
        auto cell_coord{to_cell_coord(position)};
        for (int32 axis{}; axis < n_axes; ++axis) {
            cell_coord[axis] = FMath::Clamp(cell_coord[axis], 0, max_cell_coord[axis]);
        }
        return cell_coord;
    }};
    auto const to_linear_index{[grid_width, grid_plane_stride](FIntVector3 const cell) {
        return cell.X + cell.Y * grid_width + cell.Z * grid_plane_stride;
    }};
    auto const static_aabb_indices{TConstArrayView<int32>{static_aabb_indices_}};
    auto const static_aabbs{static_aabbs_.get_const_view()};

    constexpr auto initialise_traversal_axis{[](float const cell_min,
                                                float const cell_dim,
                                                float const start,
                                                float const delta,
                                                int32& step,
                                                float& t,
                                                float& t_delta) {
        if (delta == 0.0f) {
            step = 0;
            t = TNumericLimits<float>::Max();
            t_delta = TNumericLimits<float>::Max();
            return;
        }

        step = delta > 0.0f ? 1 : -1;
        auto const next_cell_boundary{cell_min + (step > 0 ? cell_dim : 0.0f)};
        t = (next_cell_boundary - start) / delta;
        t_delta = cell_dim / FMath::Abs(delta);
    }};
    constexpr auto advance_to_next_cell{
        [](FIntVector3& cell, FIntVector3 const& steps, FVector3f& t, FVector3f const& t_deltas) {
            auto const next_t{FMath::Min3(t.X, t.Y, t.Z)};
            for (int32 axis{}; axis < 3; ++axis) {
                if (t[axis] == next_t) {
                    cell[axis] += steps[axis];
                    t[axis] += t_deltas[axis];
                }
            }
        }};
    FIntVector3 cell_padding{};
    if constexpr (TraceKind == ETraceKind::Sweep) {
        cell_padding = {
            FMath::CeilToInt(moving_half_extent.X / cell_dims_.X),
            FMath::CeilToInt(moving_half_extent.Y / cell_dims_.Y),
            FMath::CeilToInt(moving_half_extent.Z / cell_dims_.Z),
        };
    } else if constexpr (TraceKind != ETraceKind::Line) {
        static_assert(false, "Unsupported collision trace kind.");
    }

    for (int32 i_test{0}; i_test < n; ++i_test) {
        hits.hits[i_test] = 0;
        hits.entities[i_test] = FRegistryEntityHandle{};
        hits.static_geometry_indices[i_test] = INDEX_NONE;

        auto const p0{traces.starts[i_test]};
        auto const p1{traces.ends[i_test]};
        auto const delta{p1 - p0};
        FVector3f traversal_start;
        FVector3f traversal_end;

        // Restrict this trace to the grid before converting its endpoints to cell coordinates.
        // This also rejects parallel traces on the excluded positive grid boundary.
        if (!clip_segment_to_half_open_aabb(
                p0, p1, grid_min, grid_max_inside, traversal_start, traversal_end)) {
            continue;
        }

        auto const coord0{to_traversal_cell_coord(traversal_start)};
        auto const coord1{to_traversal_cell_coord(traversal_end)};
        auto const traversal_delta{traversal_end - traversal_start};
        auto current_cell{coord0};

        auto const cell_min{to_cell_min(current_cell)};
        FIntVector3 cell_steps{0, 0, 0};

        // The distance along the ray t: 0 -> 1
        FVector3f t{FVector3f::ZeroVector};

        // How far we move along an axis per cell
        FVector3f t_deltas{FVector3f::ZeroVector};
        FVector3f inv_delta{FVector3f::ZeroVector};

        for (int32 axis{}; axis < n_axes; ++axis) {
            initialise_traversal_axis(cell_min[axis],
                                      cell_dims_[axis],
                                      traversal_start[axis],
                                      traversal_delta[axis],
                                      cell_steps[axis],
                                      t[axis],
                                      t_deltas[axis]);

            if (delta[axis] != 0.0f) {
                inv_delta[axis] = 1.0f / delta[axis];
            }
        }

        auto nearest_t{std::numeric_limits<float>::infinity()};
        FRegistryEntityHandle nearest_entity;
        int32 nearest_static_index{INDEX_NONE};
        FRegistryEntityHandle ignored_entity{};
        if constexpr (IgnoredEntityMode == EIgnoredEntityMode::PerTrace) {
            ignored_entity = ignored_entities[i_test];
        } else if constexpr (IgnoredEntityMode != EIgnoredEntityMode::None) {
            static_assert(false, "Unsupported ignored entity mode.");
        }
        auto const trace_cell{[&](int32 const cell_index) {
            auto const entity_offset{cell_entity_offsets_[cell_index]};
            auto const entity_count{cell_entity_counts_[cell_index]};

            if (entity_count > 0) {
                auto const entities{TConstArrayView<FRegistryEntityHandle>{entities_}.Slice(
                    entity_offset, entity_count)};
                auto const aabbs{aabbs_.get_const_view(entity_offset, entity_count)};

                for (int32 i_entity{0}; i_entity < entity_count; ++i_entity) {
                    if constexpr (IgnoredEntityMode == EIgnoredEntityMode::PerTrace) {
                        if (entities[i_entity] == ignored_entity) {
                            continue;
                        }
                    } else if constexpr (IgnoredEntityMode != EIgnoredEntityMode::None) {
                        static_assert(false, "Unsupported ignored entity mode.");
                    }
                    if constexpr (EntityFilter == ETraceEntityFilter::ExcludeCapitalShipFighters) {
                        if (entity_registry_.get_entity_type(entities[i_entity]) ==
                            ETestEntityType::CapitalShipFighter) {
                            continue;
                        }
                    } else if constexpr (EntityFilter != ETraceEntityFilter::None) {
                        static_assert(false, "Unsupported trace entity filter.");
                    }

                    auto const hit_t{trace_aabb<TraceKind>(
                        aabbs, i_entity, p0, inv_delta, delta, moving_half_extent)};
                    if (hit_t < nearest_t) {
                        nearest_t = hit_t;
                        nearest_entity = entities[i_entity];
                        nearest_static_index = INDEX_NONE;
                    }
                }
            }

            auto const static_range_index{cell_static_range_indices_.IsValidIndex(cell_index)
                                              ? cell_static_range_indices_[cell_index]
                                              : INDEX_NONE};
            if (static_range_index == INDEX_NONE) {
                return;
            }

            auto const offset{static_cell_range_offsets_[static_range_index]};
            auto const count{static_cell_range_counts_[static_range_index]};
            auto const static_indices{
                static_aabb_indices.Slice(static_cast<int32>(offset), static_cast<int32>(count))};

            for (auto const static_index : static_indices) {
                auto const hit_t{trace_aabb<TraceKind>(
                    static_aabbs, static_index, p0, inv_delta, delta, moving_half_extent)};
                if (hit_t < nearest_t) {
                    nearest_t = hit_t;
                    nearest_entity = FRegistryEntityHandle{};
                    nearest_static_index = static_index;
                }
            }
        }};

        auto const try_advance_traversal{[&] {
            if (current_cell == coord1) {
                return false;
            }
            if (FMath::Min3(t.X, t.Y, t.Z) > 1.f) {
                return false;
            }

            advance_to_next_cell(current_cell, cell_steps, t, t_deltas);
            return true;
        }};

        if constexpr (TraceKind == ETraceKind::Sweep) {
            auto const trace_z_range{
                [&](int32 const x, int32 const y, int32 const min_z, int32 const max_z) {
                    auto cell_index{x + y * grid_width + min_z * grid_plane_stride};
                    for (int32 z{min_z}; z <= max_z; ++z) {
                        trace_cell(cell_index);
                        cell_index += grid_plane_stride;
                    }
                }};
            auto const trace_yz_plane{
                [&](int32 const x, FIntVector3 const min_cell, FIntVector3 const max_cell) {
                    for (int32 y{min_cell.Y}; y <= max_cell.Y; ++y) {
                        trace_z_range(x, y, min_cell.Z, max_cell.Z);
                    }
                }};
            auto const trace_x_range{[&](int32 const min_x,
                                         int32 const max_x,
                                         FIntVector3 const min_cell,
                                         FIntVector3 const max_cell) {
                for (int32 x{min_x}; x <= max_x; ++x) {
                    trace_yz_plane(x, min_cell, max_cell);
                }
            }};
            auto const trace_y_range{[&](int32 const x,
                                         int32 const min_y,
                                         int32 const max_y,
                                         int32 const min_z,
                                         int32 const max_z) {
                for (int32 y{min_y}; y <= max_y; ++y) {
                    trace_z_range(x, y, min_z, max_z);
                }
            }};
            auto const get_padded_cell_range{[&](FIntVector3 const centre_cell) {
                auto min_cell{centre_cell - cell_padding};
                auto max_cell{centre_cell + cell_padding};
                for (int32 axis{}; axis < n_axes; ++axis) {
                    min_cell[axis] = FMath::Max(min_cell[axis], 0);
                    max_cell[axis] = FMath::Min(max_cell[axis], max_cell_coord[axis]);
                }
                return FCellCoordBounds{min_cell, max_cell};
            }};

            auto [previous_min_cell, previous_max_cell]{get_padded_cell_range(current_cell)};
            trace_x_range(
                previous_min_cell.X, previous_max_cell.X, previous_min_cell, previous_max_cell);

            while (try_advance_traversal()) {
                auto const [min_cell, max_cell]{get_padded_cell_range(current_cell)};

                // Consecutive padded DDA boxes overlap heavily. Partition the current box into
                // disjoint X planes, Y columns, and Z ends outside the previous box so each cell
                // is traced only when it first enters the swept region.
                trace_x_range(min_cell.X,
                              FMath::Min(max_cell.X, previous_min_cell.X - 1),
                              min_cell,
                              max_cell);

                auto const overlap_min_x{FMath::Max(min_cell.X, previous_min_cell.X)};
                auto const overlap_max_x{FMath::Min(max_cell.X, previous_max_cell.X)};
                for (int32 x{overlap_min_x}; x <= overlap_max_x; ++x) {
                    trace_y_range(x,
                                  min_cell.Y,
                                  FMath::Min(max_cell.Y, previous_min_cell.Y - 1),
                                  min_cell.Z,
                                  max_cell.Z);

                    auto const overlap_min_y{FMath::Max(min_cell.Y, previous_min_cell.Y)};
                    auto const overlap_max_y{FMath::Min(max_cell.Y, previous_max_cell.Y)};
                    for (int32 y{overlap_min_y}; y <= overlap_max_y; ++y) {
                        trace_z_range(
                            x, y, min_cell.Z, FMath::Min(max_cell.Z, previous_min_cell.Z - 1));
                        trace_z_range(
                            x, y, FMath::Max(min_cell.Z, previous_max_cell.Z + 1), max_cell.Z);
                    }

                    trace_y_range(x,
                                  FMath::Max(min_cell.Y, previous_max_cell.Y + 1),
                                  max_cell.Y,
                                  min_cell.Z,
                                  max_cell.Z);
                }

                trace_x_range(FMath::Max(min_cell.X, previous_max_cell.X + 1),
                              max_cell.X,
                              min_cell,
                              max_cell);

                previous_min_cell = min_cell;
                previous_max_cell = max_cell;
            }
        } else if constexpr (TraceKind == ETraceKind::Line) {
            while (true) {
                trace_cell(to_linear_index(current_cell));
                if (!try_advance_traversal()) {
                    break;
                }
            }
        } else {
            static_assert(false, "Unsupported collision trace kind.");
        }

        if (FMath::IsFinite(nearest_t)) {
            hits.set(
                i_test, p0 + delta * nearest_t, nearest_entity, nearest_static_index, uint8{1});
        }
    }
}

auto CollisionUniformGrid::to_cell_x(float const value) const -> int32 {
    auto const half_grid_extent{static_cast<float>(grid_dims_.X) * cell_dims_.X * 0.5f};
    return to_cell(value, cell_dims_.X, half_grid_extent);
}
auto CollisionUniformGrid::to_cell_y(float const value) const -> int32 {
    auto const half_grid_extent{static_cast<float>(grid_dims_.Y) * cell_dims_.Y * 0.5f};
    return to_cell(value, cell_dims_.Y, half_grid_extent);
}
auto CollisionUniformGrid::to_cell_z(float const value) const -> int32 {
    auto const half_grid_extent{static_cast<float>(grid_dims_.Z) * cell_dims_.Z * 0.5f};
    return to_cell(value, cell_dims_.Z, half_grid_extent);
}
auto CollisionUniformGrid::to_cell_coord(FVector3f const pos) const -> FIntVector3 {
    return {
        to_cell_x(pos.X),
        to_cell_y(pos.Y),
        to_cell_z(pos.Z),
    };
}
auto CollisionUniformGrid::to_min_cell_coord(FVector3f const pos) const -> FIntVector3 {
    return to_cell_coord(pos);
}
auto CollisionUniformGrid::to_max_cell_coord(FVector3f const pos) const -> FIntVector3 {
    FVector3f const grid_dimensions{static_cast<float>(grid_dims_.X),
                                    static_cast<float>(grid_dims_.Y),
                                    static_cast<float>(grid_dims_.Z)};
    auto const half_grid_extents{grid_dimensions * cell_dims_ * 0.5f};
    return {
        to_closed_max_cell(pos.X, cell_dims_.X, grid_dims_.X, half_grid_extents.X),
        to_closed_max_cell(pos.Y, cell_dims_.Y, grid_dims_.Y, half_grid_extents.Y),
        to_closed_max_cell(pos.Z, cell_dims_.Z, grid_dims_.Z, half_grid_extents.Z),
    };
}
auto CollisionUniformGrid::to_cell_coord_bounds(FVector3f const min_point,
                                                FVector3f const max_point) const
    -> FCellCoordBounds {
    FVector3f const grid_dimensions{static_cast<float>(grid_dims_.X),
                                    static_cast<float>(grid_dims_.Y),
                                    static_cast<float>(grid_dims_.Z)};
    auto const half_grid_extents{grid_dimensions * cell_dims_ * 0.5f};

    return {
        {to_cell(min_point.X, cell_dims_.X, half_grid_extents.X),
         to_cell(min_point.Y, cell_dims_.Y, half_grid_extents.Y),
         to_cell(min_point.Z, cell_dims_.Z, half_grid_extents.Z)},
        {to_closed_max_cell(max_point.X, cell_dims_.X, grid_dims_.X, half_grid_extents.X),
         to_closed_max_cell(max_point.Y, cell_dims_.Y, grid_dims_.Y, half_grid_extents.Y),
         to_closed_max_cell(max_point.Z, cell_dims_.Z, grid_dims_.Z, half_grid_extents.Z)},
    };
}
auto CollisionUniformGrid::to_cell_min_x(int32 const x) const -> float {
    auto const half_grid_extent{static_cast<float>(grid_dims_.X) * cell_dims_.X * 0.5f};
    return (static_cast<float>(x) * cell_dims_.X) - half_grid_extent;
}
auto CollisionUniformGrid::to_cell_min_y(int32 const y) const -> float {
    auto const half_grid_extent{static_cast<float>(grid_dims_.Y) * cell_dims_.Y * 0.5f};
    return (static_cast<float>(y) * cell_dims_.Y) - half_grid_extent;
}
auto CollisionUniformGrid::to_cell_min_z(int32 const z) const -> float {
    auto const half_grid_extent{static_cast<float>(grid_dims_.Z) * cell_dims_.Z * 0.5f};
    return (static_cast<float>(z) * cell_dims_.Z) - half_grid_extent;
}
auto CollisionUniformGrid::to_cell_min(int32 const x, int32 const y, int32 const z) const
    -> FVector3f {
    return {
        to_cell_min_x(x),
        to_cell_min_y(y),
        to_cell_min_z(z),
    };
}
auto CollisionUniformGrid::to_cell_min(FIntVector3 const coord) const -> FVector3f {
    return to_cell_min(coord.X, coord.Y, coord.Z);
}
auto CollisionUniformGrid::to_cell_centre_x(int32 const x) const -> float {
    return to_cell_min_x(x) + (cell_dims_.X * 0.5f);
}
auto CollisionUniformGrid::to_cell_centre_y(int32 const y) const -> float {
    return to_cell_min_y(y) + (cell_dims_.Y * 0.5f);
}
auto CollisionUniformGrid::to_cell_centre_z(int32 const z) const -> float {
    return to_cell_min_z(z) + (cell_dims_.Z * 0.5f);
}
auto CollisionUniformGrid::to_cell_centre(int32 const x, int32 const y, int32 const z) const
    -> FVector3f {
    return {
        to_cell_centre_x(x),
        to_cell_centre_y(y),
        to_cell_centre_z(z),
    };
}
auto CollisionUniformGrid::to_cell_centre(FIntVector3 const coord) const -> FVector3f {
    return to_cell_centre(coord.X, coord.Y, coord.Z);
}
auto CollisionUniformGrid::is_cell_coord_in_bounds(FIntVector3 const coord) const -> bool {
    return coord.X >= 0 && coord.X < grid_dims_.X && coord.Y >= 0 && coord.Y < grid_dims_.Y &&
           coord.Z >= 0 && coord.Z < grid_dims_.Z;
}
auto CollisionUniformGrid::is_cell_coord_in_bounds(FIntVector3 const min_coord,
                                                   FIntVector3 const max_coord) const -> bool {
    return is_cell_coord_in_bounds(min_coord) && is_cell_coord_in_bounds(max_coord);
}
void CollisionUniformGrid::are_spheres_in_bounds(FVectors3f::ConstView const centres,
                                                 float const radius,
                                                 TArrayView<uint8> const out_results) const {
    auto const count{centres.num()};
    check(out_results.Num() == count);
    check(FMath::IsFinite(radius));
    check(radius >= 0.f);

    FVector3f const grid_dimensions{static_cast<float>(grid_dims_.X),
                                    static_cast<float>(grid_dims_.Y),
                                    static_cast<float>(grid_dims_.Z)};
    auto const half_grid_size{grid_dimensions * cell_dims_ * 0.5f};
    FVector3f const extent{radius, radius, radius};
    auto const allowed_centre_min{-half_grid_size + extent};
    auto const allowed_centre_max{half_grid_size - extent};

    for (int32 i{}; i < count; ++i) {
        auto const centre{centres[i]};
        auto const is_in_bounds{
            centre.X >= allowed_centre_min.X && centre.X <= allowed_centre_max.X &&
            centre.Y >= allowed_centre_min.Y && centre.Y <= allowed_centre_max.Y &&
            centre.Z >= allowed_centre_min.Z && centre.Z <= allowed_centre_max.Z};
        out_results[i] = static_cast<uint8>(is_in_bounds);
    }
}
auto CollisionUniformGrid::to_string(FIntVector3 const value) -> FString {
    return FString::Printf(TEXT("(%d, %d, %d)"), value.X, value.Y, value.Z);
}
auto CollisionUniformGrid::to_index(int32 const x, int32 const y, int32 const z) const -> int32 {
    return x + (y * grid_dims_.X) + (z * grid_dims_.X * grid_dims_.Y);
}
auto CollisionUniformGrid::to_index(FIntVector3 const coord) const -> int32 {
    return to_index(coord.X, coord.Y, coord.Z);
}
auto CollisionUniformGrid::to_index(FVector3f const pos) const -> int32 {
    return to_index(to_cell_coord(pos));
}
}
