#include "SpaceGame/simulation/CollisionSystem.h"

#include <SGCollision/mesh_data_extraction.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <Engine/StaticMesh.h>

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

void clear_aabb(FEntityAABBs& aabbs, int32 const index) {
    aabbs.centre_xs[index] = 0.0f;
    aabbs.centre_ys[index] = 0.0f;
    aabbs.centre_zs[index] = 0.0f;
    aabbs.half_extent_xs[index] = 0.0f;
    aabbs.half_extent_ys[index] = 0.0f;
    aabbs.half_extent_zs[index] = 0.0f;
}

void set_mesh_aabb(FEntityAABBs& aabbs,
                   int32 const index,
                   TCHAR const* const entity_name,
                   UStaticMesh const* const mesh) {
    clear_aabb(aabbs, index);

    if (!IsValid(mesh)) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("Cannot initialise collision bounds for %s: mesh is unavailable"),
               entity_name);
        return;
    }

    auto const aabb{ml::get_aabb(*mesh)};
    if (!aabb.IsValid) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("Cannot initialise collision bounds for %s: mesh %s has no valid collision "
                    "AABB"),
               entity_name,
               *mesh->GetName());
        return;
    }

    FVector3f const centre{aabb.GetCenter()};
    FVector3f const half_extents{aabb.GetExtent()};

    aabbs.centre_xs[index] = centre.X;
    aabbs.centre_ys[index] = centre.Y;
    aabbs.centre_zs[index] = centre.Z;
    aabbs.half_extent_xs[index] = half_extents.X;
    aabbs.half_extent_ys[index] = half_extents.Y;
    aabbs.half_extent_zs[index] = half_extents.Z;
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

auto CollisionUniformGrid::num_cells() const -> int32 {
    return grid_dims_.X * grid_dims_.Y * grid_dims_.Z;
}

auto CollisionUniformGrid::get_cell_entities(FIntVector3 const cell_coord) const
    -> TConstArrayView<FRegistryEntityHandle> {
    checkf(is_cell_coord_in_bounds(cell_coord),
           TEXT("Collision grid cell coordinate (%d, %d, %d) is outside grid dimensions (%d, %d, "
                "%d)"),
           cell_coord.X,
           cell_coord.Y,
           cell_coord.Z,
           grid_dims_.X,
           grid_dims_.Y,
           grid_dims_.Z);

    auto const cell_index{to_index(cell_coord)};
    return TConstArrayView<FRegistryEntityHandle>{entities_}.Slice(cell_entity_offsets_[cell_index],
                                                                   cell_entity_counts_[cell_index]);
}

void CollisionUniformGrid::reset() {
    grid_dims_ = FIntVector3::ZeroValue;
    cell_dims_ = FVector3f::ZeroVector;
    cell_entity_offsets_.Reset();
    cell_entity_counts_.Reset();
    cell_entity_write_indexes_.Reset();
    entities_.Reset();
    aabbs_.reset();
    entities_buffer_.reset();
}

void CollisionUniformGrid::rebuild_grid(FTestEntityRegistry const& entity_registry,
                                        FEntityAABBs const& entity_aabbs) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::CollisionUniformGrid::rebuild_grid);

    if (grid_dims_.X <= 0 || grid_dims_.Y <= 0 || grid_dims_.Z <= 0 || cell_dims_.X <= 0.0f ||
        cell_dims_.Y <= 0.0f || cell_dims_.Z <= 0.0f) {
        reset();
        return;
    }

    auto const& entity_data{entity_registry.get_entity_data()};
    auto const entity_count{entity_registry.get_num_elements()};
    auto const gens{entity_registry.get_generations()};

    auto const n_cells{num_cells()};

    cell_entity_counts_.Reset();
    cell_entity_counts_.AddZeroed(n_cells);

    cell_entity_offsets_.Reset();
    cell_entity_offsets_.AddZeroed(n_cells);

    entities_buffer_.reset();

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
            UE_LOG(LogSandbox,
                   Warning,
                   TEXT("Skipping collision-grid entity %d of type %s: AABB cells (%d, %d, %d) "
                        "through (%d, %d, %d) are outside grid dimensions (%d, %d, %d)"),
                   i,
                   LexToString(entity_type),
                   min_coord.X,
                   min_coord.Y,
                   min_coord.Z,
                   max_coord.X,
                   max_coord.Y,
                   max_coord.Z,
                   grid_dims_.X,
                   grid_dims_.Y,
                   grid_dims_.Z);
            continue;
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
                    ++cell_entity_counts_[cell_index];
                }
            }
        }
    }

    entities_buffer_.validate_array_sizes();

    auto const n_entries{[&] -> int32 {
        int32 offset{0};

        for (int32 i{0}; i < n_cells; ++i) {
            cell_entity_offsets_[i] = offset;
            offset += cell_entity_counts_[i];
        }
        return offset;
    }()};

    aabbs_.reset();
    aabbs_.add_uninitialised(n_entries);

    entities_.Reset();
    entities_.AddUninitialized(n_entries);

    cell_entity_write_indexes_ = cell_entity_offsets_;

    auto const buffer_count{entities_buffer_.num()};
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

    for (int32 i{0}; i < n_cells; ++i) {
        auto const write_index{cell_entity_write_indexes_[i]};
        auto const expected{cell_entity_offsets_[i] + cell_entity_counts_[i]};

        if (write_index != expected) {
            UE_LOG(LogSandbox,
                   Fatal,
                   TEXT("Index incorrect. Got %d, should be %d"),
                   write_index,
                   expected);
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
auto CollisionUniformGrid::to_min_cell_coord(FVector3f const pos) const -> FIntVector3 {
    return {
        to_cell_x(pos.X),
        to_cell_y(pos.Y),
        to_cell_z(pos.Z),
    };
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
auto CollisionUniformGrid::is_cell_coord_in_bounds(FIntVector3 const coord) const -> bool {
    return coord.X >= 0 && coord.X < grid_dims_.X && coord.Y >= 0 && coord.Y < grid_dims_.Y &&
           coord.Z >= 0 && coord.Z < grid_dims_.Z;
}
auto CollisionUniformGrid::to_index(int32 const x, int32 const y, int32 const z) const -> int32 {
    return x + (y * grid_dims_.X) + (z * grid_dims_.X * grid_dims_.Y);
}
auto CollisionUniformGrid::to_index(FIntVector3 const coord) const -> int32 {
    return to_index(coord.X, coord.Y, coord.Z);
}
auto CollisionUniformGrid::to_index(FVector3f const pos) const -> int32 {
    return to_index(to_min_cell_coord(pos));
}

void FCollisionSystem::initialise(EntityMeshes const& meshes) {
    auto const count{FEntityAABBs::num()};
    for (int32 i{0}; i < count; ++i) {
        auto const entity_type{static_cast<ETestEntityType>(i)};
        set_mesh_aabb(entity_aabbs_, i, LexToString(entity_type), meshes[i]);
    }
}

void FCollisionSystem::update() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCollisionSystem::update);
    rebuild_grid();
}

void FCollisionSystem::set_entity_registry(FTestEntityRegistry const& registry) {
    entity_registry_ = &registry;
}

void FCollisionSystem::rebuild_grid() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCollisionSystem::rebuild_grid);

    if (entity_registry_ == nullptr) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("Cannot rebuild the collision grid: entity registry is unavailable"));
        return;
    }

    uniform_grid_.rebuild_grid(*entity_registry_, entity_aabbs_);
}
}
