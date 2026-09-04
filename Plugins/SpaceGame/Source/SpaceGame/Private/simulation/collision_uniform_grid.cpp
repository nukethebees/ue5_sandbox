#include "SpaceGame/simulation/collision_uniform_grid.h"

#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/simulation/LineTraces.h>
#include <SpaceGame/simulation/TraceHits.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

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

void CollisionUniformGrid::set_entity_registry(FTestEntityRegistry const& reg) noexcept {
    entity_registry_ = &reg;
}

auto CollisionUniformGrid::is_configured() const noexcept -> bool {
    if (grid_dims_.X <= 0 || grid_dims_.Y <= 0 || grid_dims_.Z <= 0 || cell_dims_.X <= 0.0f ||
        cell_dims_.Y <= 0.0f || cell_dims_.Z <= 0.0f) {
        return false;
    }

    if (entity_registry_ == nullptr) {
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

    auto const& entity_data{entity_registry_->get_entity_data()};
    auto const entity_count{entity_registry_->get_num_elements()};
    auto const gens{entity_registry_->get_generations()};

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
            auto const local_aabb_centre{entity_aabbs.get_centre(aabb_index)};
            auto const half_extents{entity_aabbs.get_half_extents(aabb_index)};
            auto const world_aabb_centre{entity_location + local_aabb_centre};
            auto const min_point{world_aabb_centre - half_extents};
            auto const max_point{world_aabb_centre + half_extents};

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

                        aabbs_.mins.set(write_index, min_point);
                        aabbs_.maxes.set(write_index, max_point);
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

void CollisionUniformGrid::trace_aabbs(
    FLineTracesConstView const& traces,
    FTraceHitsView const& hits,
    TConstArrayView<FRegistryEntityHandle> const ignored_entities) const {
    constexpr int32 n_axes{3};

    auto const n{traces.num()};
    check(n == hits.num());
    check(ignored_entities.IsEmpty() || ignored_entities.Num() == n);

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
    auto const to_traversal_cell_coord{[this](FVector3f const position) {
        auto cell_coord{to_cell_coord(position)};
        for (int32 axis{}; axis < n_axes; ++axis) {
            cell_coord[axis] = FMath::Clamp(cell_coord[axis], 0, grid_dims_[axis] - 1);
        }
        return cell_coord;
    }};

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
    constexpr auto trace_entity{[](WorldAABBs::ConstView const& aabbs,
                                   int32 const i_entity,
                                   FVector3f const p0,
                                   FVector3f const inv_delta,
                                   FVector3f const delta) -> float {
        constexpr auto no_hit{std::numeric_limits<float>::infinity()};

        // We want to find the hit with the smallest tmin
        // That is the closest hit
        float tmin{0.f};
        // We don't want to find anything that is beyond the ray
        float tmax{1.f};

        auto const aabb_min{aabbs.mins[i_entity]};
        auto const aabb_max{aabbs.maxes[i_entity]};

        for (int32 axis{0}; axis < n_axes; ++axis) {
            auto const slab_min{aabb_min[axis]};
            auto const slab_max{aabb_max[axis]};
            auto const start{p0[axis]};
            auto const axis_delta{delta[axis]};

            if (axis_delta == 0.0f) {
                if (start < slab_min || start > slab_max) {
                    return no_hit;
                }

                continue;
            }

            auto const inverse_axis_delta{inv_delta[axis]};
            auto t1{(slab_min - start) * inverse_axis_delta};
            auto t2{(slab_max - start) * inverse_axis_delta};

            if (t1 > t2) {
                Swap(t1, t2);
            }

            // Compute the intersection of slab intersection intervals
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);

            // Exit with no collision as soon as slab intersection becomes empty
            if (tmin > tmax) {
                return no_hit;
            }
        }

        return tmin;
    }};

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
        auto const ignored_entity{ignored_entities.IsEmpty() ? FRegistryEntityHandle{}
                                                             : ignored_entities[i_test]};

        while (true) {
            auto const cell_index{to_index(current_cell)};

            auto const entity_offset{cell_entity_offsets_[cell_index]};
            auto const entity_count{cell_entity_counts_[cell_index]};

            if (entity_count > 0) {
                auto const entities{TConstArrayView<FRegistryEntityHandle>{entities_}.Slice(
                    entity_offset, entity_count)};
                auto const aabbs{aabbs_.get_const_view(entity_offset, entity_count)};

                for (int32 i_entity{0}; i_entity < entity_count; ++i_entity) {
                    if (entities[i_entity] == ignored_entity) {
                        continue;
                    }

                    auto const hit_t{trace_entity(aabbs, i_entity, p0, inv_delta, delta)};
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
            if (static_range_index != INDEX_NONE) {
                auto const offset{static_cell_range_offsets_[static_range_index]};
                auto const count{static_cell_range_counts_[static_range_index]};
                auto const static_indices{TConstArrayView<int32>{static_aabb_indices_}.Slice(
                    static_cast<int32>(offset), static_cast<int32>(count))};
                auto const static_aabbs{static_aabbs_.get_const_view()};

                for (auto const static_index : static_indices) {
                    auto const hit_t{
                        trace_entity(static_aabbs, static_index, p0, inv_delta, delta)};
                    if (hit_t < nearest_t) {
                        nearest_t = hit_t;
                        nearest_entity = FRegistryEntityHandle{};
                        nearest_static_index = static_index;
                    }
                }
            }

            if (current_cell == coord1) {
                break;
            }

            advance_to_next_cell(current_cell, cell_steps, t, t_deltas);
            if (!is_cell_coord_in_bounds(current_cell)) {
                break;
            }
        }

        if (FMath::IsFinite(nearest_t)) {
            hits.locations.set(i_test, p0 + delta * nearest_t);
            hits.entities[i_test] = nearest_entity;
            hits.static_geometry_indices[i_test] = nearest_static_index;
            hits.hits[i_test] = uint8{1};
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
