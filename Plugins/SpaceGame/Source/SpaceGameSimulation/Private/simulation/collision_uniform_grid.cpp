#include "SpaceGameSimulation/simulation/collision_uniform_grid.h"

#include <sandbox/simulation/collision_grid.h>
#include <sandbox/simulation/collision_grid_overlap_query.h>
#include <sandbox/simulation/entity_cell_data_operations.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SpaceGameSimulation/entities/NativeEntityRegistryView.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/entities/TestEntityType.h>
#include <SpaceGameSimulation/simulation/EntityWorldBounds.h>
#include <SpaceGameSimulation/simulation/LineTraces.h>
#include <SpaceGameSimulation/simulation/NativeVectorTypes.h>
#include <SpaceGameSimulation/simulation/TraceHits.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

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

auto to_native(FIntVector3 const value) noexcept -> simulation::collision::CellCoord {
    return {value.X, value.Y, value.Z};
}
auto to_unreal(simulation::collision::CellCoord const value) noexcept -> FIntVector3 {
    return {value.x, value.y, value.z};
}
}

auto CollisionUniformGrid::get_grid_dims() const noexcept -> FIntVector3 {
    return to_unreal(geometry_.dimensions);
}
void CollisionUniformGrid::set_grid_dims(FIntVector3 const grid_dims) noexcept {
    geometry_.dimensions = to_native(grid_dims);
}

auto CollisionUniformGrid::get_cell_dims() const noexcept -> FVector3f {
    return ml::to_unreal(geometry_.cell_dimensions);
}
void CollisionUniformGrid::set_cell_dims(FVector3f const cell_dims) noexcept {
    geometry_.cell_dimensions = ml::to_native(cell_dims);
}

CollisionUniformGrid::CollisionUniformGrid(FTestEntityRegistry const& entity_registry) noexcept
    : entity_registry_{entity_registry} {}

auto CollisionUniformGrid::is_configured() const noexcept -> bool {
    return simulation::collision::is_configured(geometry_);
}

auto CollisionUniformGrid::num_cells() const -> int32 {
    return simulation::collision::num_cells(geometry_);
}
auto CollisionUniformGrid::get_cell_entities(FIntVector3 const cell_coord) const
    -> std::span<FRegistryEntityHandle const> {
    checkf(is_cell_coord_in_bounds(cell_coord),
           TEXT("Collision grid cell coordinate %s is outside grid dimensions %s"),
           *to_string(cell_coord),
           *to_string(get_grid_dims()));

    auto const cell_index{to_index(cell_coord)};
    return entity_storage_.entities_for_cell(cell_index);
}

void CollisionUniformGrid::reset() {
    geometry_ = {};
    entity_storage_.reset();
    static_storage_.reset();
}

void CollisionUniformGrid::set_static_aabbs(WorldAABBs static_aabbs) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::CollisionUniformGrid::set_static_aabbs);

    if (!is_configured()) {
        UE_LOG(LogSandbox, Fatal, TEXT("Cannot build static geometry for an unconfigured grid"));
    }

    static_aabbs.get_const_view().columns().validate_array_sizes();
    static_storage_.set_aabbs(MoveTemp(static_aabbs));
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

    auto const static_index{
        static_storage_.add_aabb(ml::to_native(min_point), ml::to_native(max_point))};
    rebuild_static_grid();
    return static_index;
}

void CollisionUniformGrid::rebuild_static_grid() {
    auto const result{static_storage_.rebuild(geometry_)};
    if (result) {
        return;
    }

    auto const error{result.error()};
    switch (error.code) {
        case simulation::collision::StaticGridBuildErrorCode::AabbOutOfBounds: {
            UE_LOG(LogSandbox,
                   Fatal,
                   TEXT("Static collision AABB %d is outside the collision grid"),
                   error.aabb_index);
            return;
        }
        case simulation::collision::StaticGridBuildErrorCode::CellCountOverflow: {
            UE_LOG(LogSandbox,
                   Fatal,
                   TEXT("Static collision cell %d contains %lld AABBs, exceeding uint16 capacity"),
                   error.cell_index,
                   error.count);
            return;
        }
        case simulation::collision::StaticGridBuildErrorCode::MembershipCountOverflow: {
            UE_LOG(LogSandbox, Fatal, TEXT("Static collision grid contains too many memberships"));
            return;
        }
        case simulation::collision::StaticGridBuildErrorCode::InconsistentWriteIndex: {
            UE_LOG(LogSandbox,
                   Fatal,
                   TEXT("Static collision grid range %d has an inconsistent write index"),
                   error.cell_index);
            return;
        }
    }
}

void CollisionUniformGrid::rebuild_grid(FEntityAABBs const& entity_aabbs) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::CollisionUniformGrid::rebuild_grid);

    telemetry_.record_rebuild();

    if (!is_configured()) {
        UE_LOG(LogSandbox,
               Fatal,
               TEXT("Cannot rebuild unconfigured collision grid: cell dimensions are (%g, %g, "
                    "%g), grid dimensions are %s"),
               geometry_.cell_dimensions.X,
               geometry_.cell_dimensions.Y,
               geometry_.cell_dimensions.Z,
               *to_string(get_grid_dims()));
    }

    auto const& entity_data{entity_registry_.get_entity_data()};
    auto const entity_count{entity_registry_.get_num_elements()};
    auto const gens{entity_registry_.get_generations()};

    auto const geometry{geometry_};
    entity_storage_.begin_rebuild(geometry_.dimensions);

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
            auto const min_point{bounds.min};
            auto const max_point{bounds.max};
            auto const [min_coord, max_coord]{
                simulation::collision::to_cell_coord_bounds(geometry, min_point, max_point)};

            if (!simulation::collision::is_cell_coord_in_bounds(geometry, min_coord) ||
                !simulation::collision::is_cell_coord_in_bounds(geometry, max_coord)) {
                auto const grid_dims{get_grid_dims()};
                auto const cell_dims{get_cell_dims()};
                FVector3f const grid_dimensions{static_cast<float>(grid_dims.X),
                                                static_cast<float>(grid_dims.Y),
                                                static_cast<float>(grid_dims.Z)};
                auto const half_grid_size{grid_dimensions * cell_dims * 0.5f};
                auto const unreal_min_point{ml::to_unreal(min_point)};
                auto const unreal_max_point{ml::to_unreal(max_point)};
                UE_LOG(
                    LogSandbox,
                    Fatal,
                    TEXT("Collision-grid entity %s of type %s has world AABB (%s) through (%s), "
                         "cell AABB %s through %s, outside grid world bounds (%s) through (%s) and "
                         "cell bounds %s through %s"),
                    *LexToString(FRegistryEntityHandle{i, gens[i]}),
                    LexToString(entity_type),
                    *unreal_min_point.ToString(),
                    *unreal_max_point.ToString(),
                    *to_string(to_unreal(min_coord)),
                    *to_string(to_unreal(max_coord)),
                    *(-half_grid_size).ToString(),
                    *half_grid_size.ToString(),
                    *to_string(FIntVector3::ZeroValue),
                    *to_string(grid_dims - FIntVector3{1, 1, 1}));
            }

            entity_storage_.add(min_point, max_point, min_coord, max_coord, {i, gens[i]});
        }
    }

    if (!entity_storage_.finish_rebuild()) {
        UE_LOG(LogSandbox, Fatal, TEXT("Collision grid entity membership index is inconsistent"));
    }
}

void CollisionUniformGrid::append_overlaps(simulation::collision::WorldAABB const& query_bounds,
                                           FRegistryEntityHandle const ignored_entity,
                                           std::vector<FRegistryEntityHandle>& out_entities,
                                           std::vector<int32>& out_static_geometry_indices) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::CollisionUniformGrid::append_overlaps);

    auto const geometry{geometry_};
    auto const [min_coord, max_coord]{
        simulation::collision::to_cell_coord_bounds(geometry, query_bounds.min, query_bounds.max)};
    checkf(simulation::collision::is_cell_coord_in_bounds(geometry, min_coord) &&
               simulation::collision::is_cell_coord_in_bounds(geometry, max_coord),
           TEXT("AABB query (%s) through (%s) is outside collision grid dimensions %s"),
           *ml::to_unreal(query_bounds.min).ToString(),
           *ml::to_unreal(query_bounds.max).ToString(),
           *to_string(get_grid_dims()));

    simulation::collision::append_grid_overlaps(geometry_,
                                                entity_storage_,
                                                static_storage_,
                                                ml::make_native_query_view(entity_registry_),
                                                query_bounds,
                                                ignored_entity,
                                                out_entities,
                                                out_static_geometry_indices);
}

void CollisionUniformGrid::trace_aabbs(FLineTracesConstView const& traces,
                                       FTraceHitsView const& hits) const {
    telemetry_.record_line_traces(static_cast<uint64>(traces.num()));
    simulation::collision::trace_grid_lines(geometry_,
                                            entity_storage_,
                                            static_storage_,
                                            ml::make_native_query_view(entity_registry_),
                                            traces,
                                            hits);
}

void CollisionUniformGrid::trace_aabbs(
    FLineTracesConstView const& traces,
    FTraceHitsView const& hits,
    TConstArrayView<FRegistryEntityHandle> const ignored_entities) const {
    telemetry_.record_line_traces(static_cast<uint64>(traces.num()));
    simulation::collision::trace_grid_lines_ignoring_entities(
        geometry_,
        entity_storage_,
        static_storage_,
        ml::make_native_query_view(entity_registry_),
        traces,
        hits,
        {ignored_entities.GetData(), static_cast<std::size_t>(ignored_entities.Num())});
}

void
    CollisionUniformGrid::sweep_aabbs(FLineTracesConstView const& centre_paths,
                                      FVector3f const moving_half_extent,
                                      FTraceHitsView const& hits,
                                      TConstArrayView<FRegistryEntityHandle> const ignored_entities,
                                      ETraceEntityFilter const entity_filter) const {
    telemetry_.record_sweep_traces(static_cast<uint64>(centre_paths.num()));
    check(!moving_half_extent.ContainsNaN());
    check(moving_half_extent.X >= 0.f);
    check(moving_half_extent.Y >= 0.f);
    check(moving_half_extent.Z >= 0.f);
    simulation::collision::sweep_grid_aabbs(
        geometry_,
        entity_storage_,
        static_storage_,
        ml::make_native_query_view(entity_registry_),
        centre_paths,
        ml::to_native(moving_half_extent),
        hits,
        {ignored_entities.GetData(), static_cast<std::size_t>(ignored_entities.Num())},
        entity_filter);
}

void CollisionUniformGrid::reset_runtime_telemetry() noexcept {
    telemetry_.reset();
}

auto CollisionUniformGrid::get_runtime_telemetry() const noexcept
    -> FCollisionGridTelemetrySnapshot {
    return telemetry_.snapshot();
}

auto CollisionUniformGrid::to_cell_x(float const value) const -> int32 {
    return simulation::collision::to_cell_coord(
        value, geometry_.cell_dimensions.X, geometry_.dimensions.x);
}
auto CollisionUniformGrid::to_cell_y(float const value) const -> int32 {
    return simulation::collision::to_cell_coord(
        value, geometry_.cell_dimensions.Y, geometry_.dimensions.y);
}
auto CollisionUniformGrid::to_cell_z(float const value) const -> int32 {
    return simulation::collision::to_cell_coord(
        value, geometry_.cell_dimensions.Z, geometry_.dimensions.z);
}
auto CollisionUniformGrid::to_cell_coord(FVector3f const pos) const -> FIntVector3 {
    return to_unreal(simulation::collision::to_cell_coord(geometry_, ml::to_native(pos)));
}
auto CollisionUniformGrid::to_min_cell_coord(FVector3f const pos) const -> FIntVector3 {
    return to_cell_coord(pos);
}
auto CollisionUniformGrid::to_max_cell_coord(FVector3f const pos) const -> FIntVector3 {
    return to_unreal(simulation::collision::to_max_cell_coord(geometry_, ml::to_native(pos)));
}
auto CollisionUniformGrid::to_cell_coord_bounds(FVector3f const min_point,
                                                FVector3f const max_point) const
    -> FCellCoordBounds {
    auto const bounds{simulation::collision::to_cell_coord_bounds(
        geometry_, ml::to_native(min_point), ml::to_native(max_point))};
    return {to_unreal(bounds.min), to_unreal(bounds.max)};
}
auto CollisionUniformGrid::to_cell_min_x(int32 const x) const -> float {
    return simulation::collision::to_cell_min(
        x, geometry_.cell_dimensions.X, geometry_.dimensions.x);
}
auto CollisionUniformGrid::to_cell_min_y(int32 const y) const -> float {
    return simulation::collision::to_cell_min(
        y, geometry_.cell_dimensions.Y, geometry_.dimensions.y);
}
auto CollisionUniformGrid::to_cell_min_z(int32 const z) const -> float {
    return simulation::collision::to_cell_min(
        z, geometry_.cell_dimensions.Z, geometry_.dimensions.z);
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
    return ml::to_unreal(simulation::collision::to_cell_min(geometry_, to_native(coord)));
}
auto CollisionUniformGrid::to_cell_centre_x(int32 const x) const -> float {
    return to_cell_min_x(x) + (geometry_.cell_dimensions.X * 0.5f);
}
auto CollisionUniformGrid::to_cell_centre_y(int32 const y) const -> float {
    return to_cell_min_y(y) + (geometry_.cell_dimensions.Y * 0.5f);
}
auto CollisionUniformGrid::to_cell_centre_z(int32 const z) const -> float {
    return to_cell_min_z(z) + (geometry_.cell_dimensions.Z * 0.5f);
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
    return ml::to_unreal(simulation::collision::to_cell_centre(geometry_, to_native(coord)));
}
auto CollisionUniformGrid::is_cell_coord_in_bounds(FIntVector3 const coord) const -> bool {
    return simulation::collision::is_cell_coord_in_bounds(geometry_, to_native(coord));
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

    simulation::collision::are_spheres_in_bounds(
        geometry_,
        ml::to_native(centres),
        radius,
        {out_results.GetData(), static_cast<std::size_t>(out_results.Num())});
}
auto CollisionUniformGrid::to_string(FIntVector3 const value) -> FString {
    return FString::Printf(TEXT("(%d, %d, %d)"), value.X, value.Y, value.Z);
}
auto CollisionUniformGrid::to_index(int32 const x, int32 const y, int32 const z) const -> int32 {
    return simulation::collision::to_index(geometry_, {x, y, z});
}
auto CollisionUniformGrid::to_index(FIntVector3 const coord) const -> int32 {
    return to_index(coord.X, coord.Y, coord.Z);
}
auto CollisionUniformGrid::to_index(FVector3f const pos) const -> int32 {
    return to_index(to_cell_coord(pos));
}
}
