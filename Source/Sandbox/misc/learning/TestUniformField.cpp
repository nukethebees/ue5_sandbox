#include "TestUniformField.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/actor_utils.h"
#include "Sandbox/utilities/macros/null_checks.hpp"
#include "Sandbox/utilities/world.h"

#include "Async/ParallelFor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#include <algorithm>
#include <limits>

namespace ml {
auto QuantisedPotential::get_strength(float strength) -> Value {
    return std::min(max_strength_coord, static_cast<Value>(strength / 0.1f));
}
auto QuantisedPotential::get_axis_section(float coord) -> Value {
    check(coord <= 1.f);
    check(coord >= -1.f);

    constexpr float scale{grid_side_length};

    // translate from [-1, 1] to [0, 3]
    auto const c{(coord + 1.f) * 1.5f};

    return std::min(max_grid_side_coord, static_cast<Value>(c * scale));
}
auto QuantisedPotential::get_direction_index(Value x, Value y, Value z) -> Value {
    constexpr auto len{grid_side_length};
    constexpr auto len_sq{len * len};

    return z + (y * len) + (x * len_sq);
}
}

bool FTestUniformFieldCell::quantised_changed() const {
    return old_quantised_potential != new_quantised_potential;
}
void FTestUniformFieldCell::reset() {
    potential = FVector3f::ZeroVector;
}

ATestUniformField::ATestUniformField()
    : box_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("box_mesh"))}
    , vector_meshes{CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
          TEXT("vector_meshes"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    box_mesh->SetupAttachment(RootComponent);
    vector_meshes->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    // Run when everyone else is finished
    PrimaryActorTick.TickGroup = ETickingGroup::TG_PostPhysics;
}

// Actor lifecycle
void ATestUniformField::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    reset();
    default_construct();
}
void ATestUniformField::BeginPlay() {
    Super::BeginPlay();

    check(vector_meshes->GetStaticMesh());

    reset();
    default_construct();

    TRY_INIT_PTR(world, GetWorld());
    on_world_match_starting_handle =
        world->OnWorldMatchStarting.AddUObject(this, &ThisClass::on_world_match_starting);
}
void ATestUniformField::on_world_match_starting() {
    UE_LOG(LogSandboxLearning, Verbose, TEXT("ATestUniformField::on_world_match_starting"));

    UE_LOG(LogSandboxLearning,
           Verbose,
           TEXT("%s::on_world_match_starting"),
           *ml::get_best_display_name(*this));

    on_field_pre_construction.Broadcast(*this);

    update_cells();
    update_visualisation();

    on_field_post_construction.Broadcast(*this);
}
void ATestUniformField::Tick(float dt) {
    Super::Tick(dt);

#if WITH_EDITOR
    dbg_timer.tick(dt);
#endif

    update_cells();
    update_visualisation();

#if WITH_EDITOR
    if (dbg_timer.is_finished()) {
        dbg_timer.reset();
    }
#endif

    SetActorTickEnabled(false);
}
void ATestUniformField::EndPlay(EEndPlayReason::Type const reason) {
    auto* world{GetWorld()};
    if (world) {
        world->OnWorldMatchStarting.Remove(on_world_match_starting_handle);
        reset();
    } else {
        UE_LOG(LogSandboxLearning, Warning, TEXT("world is nullptr"));
    }

    Super::EndPlay(reason);
}

auto ATestUniformField::sample_field(FVector const& position) const -> FTestUniformFieldCell {
    auto const i{get_index(position)};
    if (cells.IsValidIndex(i)) {
        return cells[i];
    } else {
        return {};
    }
}

// Source registration
auto ATestUniformField::add_source(FTestUniformFieldPointSourceData const& source) -> int32 {
    int const i{point_sources.Num()};
    point_sources.Add(source);
    mark_all_dirty();

    return i;
}
auto ATestUniformField::add_sources(TArrayView<FTestUniformFieldPointSourceData const> sources)
    -> int32 {
    int const i{point_sources.Num()};

    for (auto const& src : sources) {
        point_sources.Add(src);
    }

    mark_all_dirty();

    return i;
}
void ATestUniformField::update_source(FTestUniformFieldPointSourceData const& source, int32 i) {
    point_sources[i] = source;
    mark_all_dirty();
}
void ATestUniformField::update_sources(TArrayView<FTestUniformFieldPointSourceData const> sources,
                                       int32 offset) {
    auto const n{sources.Num()};
    for (int32 i{0}; i < n; ++i) {
        point_sources[offset + i] = sources[i];
    }
    mark_all_dirty();
}

// Field construction
void ATestUniformField::default_construct() {
    construct_grid();
    configure_box_mesh();
    configure_hism();
    initialise_hism_visualisation();
}
void ATestUniformField::construct_grid() {
    grid_dimensions = FIntVector{box_extent / cell_extent};

    auto const n{get_num_cells()};
    cells.AddDefaulted(n);

#if WITH_EDITOR
    dbg_num_cells = n;
#endif
}

// Field update
void ATestUniformField::update_cells() {
    if (!grid_dirty) {
        return;
    }

    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("ATestUniformField::update_cells"));

    auto const n_cells{get_num_cells()};
    auto const n_sources{point_sources.Num()};
    auto const origin{get_origin_cell_centre()};

    reset_cells_to_default();

    for (auto const& ps : point_sources) {
        auto const source_pos{ps.coordinate};
        auto const inner_radius_m{ps.inner_radius / 100.f};

        auto compute_cell{[=, this](int32 i) -> void {
            auto const delta_pos{this->get_position_from_origin_cell_centre(i)};
            auto const cell_pos{origin + delta_pos};
            auto const displacement{cell_pos - source_pos};
            auto const dist_cm{static_cast<float>(displacement.Length())};
            auto const dist_m{dist_cm / 100.f};

            auto const dist_from_radius_m{dist_m - inner_radius_m};

            auto const within_radii{(dist_cm > ps.inner_radius) && (dist_cm < ps.outer_radius)};

            if (!within_radii) {
                return;
            }

            auto const strength{ps.strength * FMath::Pow(dist_from_radius_m, ps.falloff)};
            auto const dir{
                static_cast<FVector3f>(ps.rotation.RotateVector(displacement).GetSafeNormal())};
            auto const potential{dir * strength};

            auto& cell{cells[i]};
            cell.potential += potential;
        }};

        ParallelFor(n_cells, compute_cell);
    }

    for (auto& cell : cells) {
        max_abs_strength = std::max(max_abs_strength, cell.potential.SquaredLength());
    }
    max_abs_strength = FMath::Sqrt(max_abs_strength);

    ParallelFor(n_cells, [this](int32 i) -> void {
        using QP = ml::QuantisedPotential;

        auto& cell{this->cells[i]};
        cell.old_quantised_potential = cell.new_quantised_potential;

        auto const strength{cell.potential.Size()};

        cell.new_quantised_potential.strength = QP::get_strength(strength);

        auto const dir{cell.potential.GetSafeNormal()};
        auto const x{QP::get_axis_section(dir.X)};
        auto const y{QP::get_axis_section(dir.Y)};
        auto const z{QP::get_axis_section(dir.Z)};

        cell.new_quantised_potential.direction = QP::get_direction_index(x, y, z);
    });

    grid_dirty = false;
}
void ATestUniformField::reset_cells_to_default() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("ATestUniformField::reset_cells"));

    for (auto& cell : cells) {
        cell.reset();
    }

    max_abs_strength = 0.f;
}

// Visualiation - configuration
void ATestUniformField::configure_visualisation_component(UStaticMeshComponent& mc) {
    // Collision
    mc.SetCollisionEnabled(ECollisionEnabled::NoCollision);
    mc.SetCollisionResponseToAllChannels(ECR_Ignore);
    mc.SetGenerateOverlapEvents(false);
    mc.SetAllUseCCD(false);
    mc.SetAllUseMACD(false);
    mc.SetEnableGravity(false);

    mc.SetCastShadow(false);
    mc.bCastDynamicShadow = false;
    mc.bCastStaticShadow = false;
    mc.SetAffectDistanceFieldLighting(false);
    mc.SetAffectDynamicIndirectLighting(false);
    mc.SetAffectIndirectLightingWhileHidden(false);

    mc.SetCanEverAffectNavigation(false);
}
void ATestUniformField::configure_box_mesh() {
    configure_visualisation_component(*box_mesh);

    box_mesh->SetVisibility(display_box);
    auto box_mesh_src{box_mesh->GetStaticMesh()};
    if (!box_mesh_src) {
        return;
    }

    box_mesh->SetRelativeScale3D(get_field_extent() / box_mesh_src->GetBounds().BoxExtent);
}
void ATestUniformField::configure_hism() {
    auto& hism{*vector_meshes};

    hism.ClearInstances();
    hism.SetNumCustomDataFloats(1); // Must be set before preallocation
    hism.PreAllocateInstancesMemory(get_num_cells());

    configure_visualisation_component(hism);
}

// Visualisation - initialisation
void ATestUniformField::initialise_hism_visualisation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("ATestUniformField::initialise_hism_visualisation"));

    auto& hism{*vector_meshes};
    update_hism_visibility();

    auto const num_cells{get_num_cells()};
    auto const origin{get_origin_cell_centre()};

    vector_transforms.SetNumUninitialized(num_cells, EAllowShrinking::No);
    vector_intensities.SetNumUninitialized(num_cells, EAllowShrinking::No);
    update_hism_data(origin, 0, num_cells);

    constexpr bool return_indices{false};
    constexpr bool world_space{true};
    constexpr bool update_nav{false};

    hism.AddInstances(vector_transforms, return_indices, world_space, update_nav);

    for (int32 i{0}; i < num_cells; ++i) {
        hism.SetCustomDataValue(i, 0, vector_intensities[i], false);
    }

    hism.MarkRenderStateDirty();

    visualisation_dirty = false;
}

// Visualisation - updates
void ATestUniformField::update_visualisation() {
    update_hism_visualisation();
}
void ATestUniformField::update_hism_visualisation() {
    if (!visualisation_dirty) {
        return;
    }

    constexpr bool world_space{true};
    constexpr bool mark_render_dirty{false};
    constexpr bool teleport{true};

    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("ATestUniformField::update_hism_visualisation"));

    auto& hism{*vector_meshes};
    update_hism_visibility();
    if (!display_vectors) {
        visualisation_dirty = false;
        return;
    }

    auto const num_cells{get_num_cells()};
    auto const origin{get_origin_cell_centre()};
    auto const num_hism_instances{hism.GetInstanceCount()};
    check(num_cells == num_hism_instances);

    constexpr int32 run_end_threshold{10};

    dirty_cells.Reset();
    for (int32 i{0}; i < num_cells; ++i) {
        auto& cell{cells[i]};
        if (cell.quantised_changed()) {
            dirty_cells.Emplace(i);
        }
    }

    auto const n_dirty{dirty_cells.Num()};

    if (n_dirty == 0) {
        visualisation_dirty = false;
        return;
    }

    if (n_dirty >= (num_cells / 2)) {
        update_hism_data(origin, 0, num_cells);
        hism.BatchUpdateInstancesTransforms(
            0, vector_transforms, world_space, mark_render_dirty, teleport);
        hism.PerInstanceSMCustomData = vector_intensities;
    } else {
        auto const remaining_dirty{MakeConstArrayView(dirty_cells).Slice(1, n_dirty - 1)};
        auto const n_remaining_dirty{remaining_dirty.Num()};

        dirty_runs.Reset();
        int32 run_start{dirty_cells[0]};
        int32 run_end{dirty_cells[0]};

        for (int32 i{0}; i < n_remaining_dirty; ++i) {
            auto const cell_i{remaining_dirty[i]};
            auto const dist{cell_i - run_end};

            if (dist > run_end_threshold) {
                dirty_runs.Emplace(run_start, 1 + (run_end - run_start));
                run_start = cell_i;
                run_end = cell_i;
            } else {
                run_end = cell_i;
            }
        }

        // Add final if it wasn't done
        if (run_end == run_start) {
            dirty_runs.Emplace(run_start, 1 + (run_end - run_start));
        }

        auto const n_dirty_runs{dirty_runs.Num()};

        for (auto const& run : dirty_runs) {
            ParallelFor(
                run.length,
                [=, this, offset = run.offset](int32 local_index) -> void {
                    update_hism_instance(origin, offset + local_index);
                },
                EParallelForFlags::Unbalanced);
        }

        for (auto const& run : dirty_runs) {
            auto const dirty_transform{
                MakeConstArrayView(vector_transforms).Slice(run.offset, run.length)};

            hism.BatchUpdateInstancesTransforms(
                run.offset, dirty_transform, world_space, mark_render_dirty, teleport);
        }

        hism.PerInstanceSMCustomData = vector_intensities;
        hism.BuildTreeIfOutdated(true, true);
    }

    hism.MarkRenderStateDirty();
    visualisation_dirty = false;
}
void ATestUniformField::update_hism_data(FVector const& origin, int32 offset, int32 length) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("ATestUniformField::update_hism_data"));

    ParallelFor(length, [=, this](int32 local_index) -> void {
        update_hism_instance(origin, offset + local_index);
    });
}
void ATestUniformField::update_hism_instance(FVector const& origin, int32 index) {
    auto const delta_pos{get_position_from_origin_cell_centre(index)};
    auto const pos{origin + delta_pos};

    auto const& potential{cells[index].potential};
    auto const rot_vec{FVector{potential}.Rotation()};

    auto const strength{static_cast<float>(potential.Size())};
    auto const length_scale{FMath::Max(min_length_scale, strength / max_abs_strength)};

    FVector const scale{
        length_scale * vector_base_scale.X,
        1.f * vector_base_scale.Y,
        1.f * vector_base_scale.Z,
    };

    vector_transforms[index] = FTransform{rot_vec, pos, scale};
    vector_intensities[index] = strength;
}
void ATestUniformField::update_hism_visibility() {
    vector_meshes->SetVisibility(display_vectors);
}

void ATestUniformField::mark_all_dirty() {
    mark_grid_dirty();
    mark_visualisation_dirty();
}
void ATestUniformField::mark_grid_dirty() {
    grid_dirty = true;
    SetActorTickEnabled(true);
}
void ATestUniformField::mark_visualisation_dirty() {
    visualisation_dirty = true;
    SetActorTickEnabled(true);
}

// Field geometry
auto ATestUniformField::get_coord(FVector const& pos) const -> FIntVector {
    auto const origin{get_origin_cell_centre()};
    auto const delta{pos - origin};
    auto const dimensions{get_cell_dimensions()};
    FIntVector coord{delta / dimensions};

    return coord;
}
auto ATestUniformField::get_index(FIntVector const& coord) const -> int32 {
    auto const gx{grid_dimensions.X};
    auto const gy{grid_dimensions.Y};
    auto const gz{grid_dimensions.Z};

    auto const cx{coord.X};
    auto const cy{coord.Y};
    auto const cz{coord.Z};

    return (cx * (gy * gz)) + (cy * gz) + (cz % gz);
}
auto ATestUniformField::get_index(FVector const& pos) const -> int32 {
    return get_index(get_coord(pos));
}

auto ATestUniformField::get_num_cells() const -> int32 {
    return grid_dimensions.X * grid_dimensions.Y * grid_dimensions.Z;
}
auto ATestUniformField::get_grid_coords_sum() const -> int32 {
    return grid_dimensions.X + grid_dimensions.Y + grid_dimensions.Z;
}

auto ATestUniformField::get_grid_centre() const -> FVector {
    return GetActorLocation();
}
auto ATestUniformField::get_grid_origin() const -> FVector {
    auto const loc{get_grid_centre()};

    return loc - get_field_extent();
}
auto ATestUniformField::get_origin_cell_centre() const -> FVector {
    return get_grid_origin() + get_cell_extent();
}

auto ATestUniformField::get_grid_coordinate(int32 i) const -> FIntVector {
    auto const gy{grid_dimensions.Y};
    auto const gz{grid_dimensions.Z};

    auto const x{i / gy / gz};
    auto const y{(i / gz) % gy};
    auto const z{i % gz};

    return {x, y, z};
}
auto ATestUniformField::get_position_from_origin_cell_centre(int32 i) const -> FVector {
    FVector const coord{get_grid_coordinate(i)};

    return coord * get_cell_dimensions();
}

auto ATestUniformField::get_cell_extent() const -> FVector {
    return cell_extent;
}
auto ATestUniformField::get_cell_dimensions() const -> FVector {
    return cell_extent * 2.0;
}

auto ATestUniformField::get_field_extent() const -> FVector {
    return box_extent;
}
auto ATestUniformField::get_field_dimensions() const -> FVector {
    return get_field_extent() * 2.0;
}

// Reset
void ATestUniformField::reset() {
    vector_meshes->ClearInstances();

    reset_sources();
    cells.Reset();
    reset_cells_to_default();

    vector_transforms.Reset();
    vector_intensities.Reset();
    dirty_cells.Reset();

    dirty_runs.Reset();
    dirty_runs.Reserve(50);
}
void ATestUniformField::reset_sources() {
    point_sources.Reset();
}

#if WITH_EDITOR
auto ATestUniformField::can_log() const {
    return dbg_timer.is_finished();
}
#endif
