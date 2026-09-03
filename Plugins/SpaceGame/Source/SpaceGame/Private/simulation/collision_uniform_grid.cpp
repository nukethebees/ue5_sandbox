#include "SpaceGame/simulation/collision_uniform_grid.h"

#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/simulation/LineTraces.h>
#include <SpaceGame/simulation/TraceHits.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <algorithm>
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

            auto const location{entity_data.locations[i]};

            auto const entity_type{entity_data.entity_types[i]};
            auto const half_extents{entity_aabbs.get_half_extents(std::to_underlying(entity_type))};
            auto const min_point{location - half_extents};
            auto const max_point{location + half_extents};

            auto const min_coord{to_min_cell_coord(min_point)};
            auto const max_coord{to_max_cell_coord(max_point)};

            if (!is_cell_coord_in_bounds(min_coord) || !is_cell_coord_in_bounds(max_coord)) {
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

            entities_buffer_.locations.add(location);
            entities_buffer_.min_points.add(min_point);
            entities_buffer_.max_points.add(max_point);
            entities_buffer_.mins.add(min_coord);
            entities_buffer_.maxes.add(max_coord);
            entities_buffer_.entity_types.Add(entity_type);
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

void CollisionUniformGrid::trace_aabbs(FLineTracesConstView const& traces,
                                       FTraceHitsView const& hits) const {
    constexpr int32 n_axes{3};

    auto const n{traces.num()};
    check(n == hits.num());

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
        // We want to find the hit with the smallest tmin
        // That is the closest hit
        float tmin{0.f};
        // We don't want to find anything that is beyond the ray
        float tmax{1.f};

        for (int32 axis{0}; axis < n_axes; ++axis) {
            auto t1{(aabbs.mins[i_entity][axis] - p0[axis]) * inv_delta[axis]};
            auto t2{(aabbs.maxes[i_entity][axis] - p0[axis]) * inv_delta[axis]};

            if (t1 > t2) {
                Swap(t1, t2);
            }

            // Compute the intersection of slab intersection intervals
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);

            // Exit with no collision as soon as slab intersection becomes empty
            if (tmin > tmax) {
                return std::numeric_limits<float>::infinity();
            }
        }

        return tmin;
    }};

    for (int32 i_test{0}; i_test < n; ++i_test) {
        hits.hits[i_test] = 0;

        auto const p0{traces.starts[i_test]};
        auto const p1{traces.ends[i_test]};
        auto const coord0{to_cell_coord(p0)};
        auto const coord1{to_cell_coord(p1)};
        auto const delta{p1 - p0};
        auto current_cell{coord0};

        auto const cell_min{to_cell_min(current_cell)};
        FIntVector3 cell_steps{0, 0, 0};

        // The distance along the ray t: 0 -> 1
        FVector3f t{FVector3f::ZeroVector};

        // How far we move along an axis per cell
        FVector3f t_deltas{FVector3f::ZeroVector};

        for (int32 axis{}; axis < n_axes; ++axis) {
            initialise_traversal_axis(cell_min[axis],
                                      cell_dims_[axis],
                                      p0[axis],
                                      delta[axis],
                                      cell_steps[axis],
                                      t[axis],
                                      t_deltas[axis]);
        }

        auto nearest_t{std::numeric_limits<float>::infinity()};
        FRegistryEntityHandle nearest_entity;

        while (true) {
            auto const cell_index{to_index(current_cell)};

            auto const entity_offset{cell_entity_offsets_[cell_index]};
            auto const entity_count{cell_entity_counts_[cell_index]};

            if (entity_count > 0) {
                auto const entities{TConstArrayView<FRegistryEntityHandle>{entities_}.Slice(
                    entity_offset, entity_count)};
                auto const aabbs{aabbs_.get_const_view(entity_offset, entity_count)};

                auto const inv_delta{delta.Reciprocal()};

                for (int32 i_entity{0}; i_entity < entity_count; ++i_entity) {
                    auto const hit_t{trace_entity(aabbs, i_entity, p0, inv_delta, delta)};
                    if (hit_t < nearest_t) {
                        nearest_t = hit_t;
                        nearest_entity = entities[i_entity];
                    }
                }
            }

            if (current_cell == coord1) {
                break;
            }

            advance_to_next_cell(current_cell, cell_steps, t, t_deltas);
        }

        if (FMath::IsFinite(nearest_t)) {
            hits.locations.set(i_test, p0 + delta * nearest_t);
            hits.entities[i_test] = nearest_entity;
            hits.hits[i_test] = uint8{1};
        }
    }
}

auto CollisionUniformGrid::to_cell_x(float const value) const -> int32 {
    auto const half_grid_extent{static_cast<float>(grid_dims_.X) * cell_dims_.X * 0.5f};
    return FMath::FloorToInt((value + half_grid_extent) / cell_dims_.X);
}
auto CollisionUniformGrid::to_cell_y(float const value) const -> int32 {
    auto const half_grid_extent{static_cast<float>(grid_dims_.Y) * cell_dims_.Y * 0.5f};
    return FMath::FloorToInt((value + half_grid_extent) / cell_dims_.Y);
}
auto CollisionUniformGrid::to_cell_z(float const value) const -> int32 {
    auto const half_grid_extent{static_cast<float>(grid_dims_.Z) * cell_dims_.Z * 0.5f};
    return FMath::FloorToInt((value + half_grid_extent) / cell_dims_.Z);
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
    FVector3f const half_grid_extent{
        static_cast<float>(grid_dims_.X) * cell_dims_.X * 0.5f,
        static_cast<float>(grid_dims_.Y) * cell_dims_.Y * 0.5f,
        static_cast<float>(grid_dims_.Z) * cell_dims_.Z * 0.5f,
    };

    return {
        FMath::CeilToInt((pos.X + half_grid_extent.X) / cell_dims_.X) - 1,
        FMath::CeilToInt((pos.Y + half_grid_extent.Y) / cell_dims_.Y) - 1,
        FMath::CeilToInt((pos.Z + half_grid_extent.Z) / cell_dims_.Z) - 1,
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
