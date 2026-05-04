#include "TestUniformField.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/world.h"

#include "Async/ParallelFor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#include <algorithm>
#include <limits>

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
    PrimaryActorTick.bStartWithTickEnabled = true;
    // Run when everyone else is finished
    PrimaryActorTick.TickGroup = ETickingGroup::TG_PostPhysics;
}

void ATestUniformField::BeginPlay() {
    Super::BeginPlay();

    vector_meshes->ClearInstances();

    mark_all_dirty();
    update_cells();
    update_visualisation();
}
void ATestUniformField::Tick(float dt) {
    Super::Tick(dt);

#if WITH_EDITOR
    dbg_log_timer += dt;
#endif

    update_cells();
    update_visualisation();

#if WITH_EDITOR
    if (dbg_log_timer >= dbg_log_cooldown) {
        dbg_log_timer = 0.f;
    }
#endif
}
void ATestUniformField::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    construct_grid();
    configure_box_mesh();
    configure_hism();
}
void ATestUniformField::PostActorCreated() {
    Super::PostActorCreated();

    reset_sources();
}

auto ATestUniformField::find_field(UWorld& world) -> TWeakObjectPtr<ATestUniformField> {
    return ml::get_first_actor<ATestUniformField>(world);
}
auto ATestUniformField::sample_field(FVector const& position) const -> FTestUniformFieldCell {
    auto const i{get_index(position)};
    if (cells.IsValidIndex(i)) {
        return cells[i];
    } else {
        return {};
    }
}

void ATestUniformField::add_source(FTestUniformFieldPointSourceData const& source) {
    point_sources.Add(source);

    grid_dirty = true;
}

void ATestUniformField::construct_grid() {
    grid_dimensions = FIntVector{box_extent / cell_extent};

    auto const n{get_num_cells()};
    cells.Reset();
    cells.AddDefaulted(n);
}
void ATestUniformField::update_cells() {
    if (!grid_dirty) {
        return;
    }

    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("ATestUniformField::update_cells"));

    auto const n_cells{get_num_cells()};
    auto const origin{get_origin_cell_centre()};

    reset_cells();

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

    reset_sources();
    grid_dirty = false;
    visualisation_dirty = true;
}
void ATestUniformField::reset_cells() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("ATestUniformField::update_cells::reset_cells"));

    for (auto& cell : cells) {
        cell.reset();
    }

    constexpr auto inf{std::numeric_limits<float>::infinity()};
    max_abs_strength = 0.f;
}
void ATestUniformField::reset_sources() {
    point_sources.Reset();
}

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

    configure_visualisation_component(hism);

    hism.SetNumCustomDataFloats(1);
    vector_meshes->ClearInstances();
}

void ATestUniformField::update_visualisation() {
    update_hism_visualisation();
}
void ATestUniformField::update_hism_visualisation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("ATestUniformField::update_hism_visualisation"));

    auto& hism{*vector_meshes};

    hism.SetVisibility(display_vectors);
    if (!display_vectors || !visualisation_dirty) {
        return;
    }

    auto const mesh{hism.GetStaticMesh()};
    if (!mesh) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("vector_meshes has no static mesh."))
        return;
    }

    auto const num_cells{get_num_cells()};
    auto const origin{get_origin_cell_centre()};

    vector_transforms.Reset();
    vector_transforms.Reserve(num_cells);
    vector_intensities.Reset();
    vector_intensities.Reserve(num_cells);
    vector_intensities.AddDefaulted(num_cells);

    {
        TRACE_CPUPROFILER_EVENT_SCOPE(
            TEXT("ATestUniformField::update_hism_visualisation::transform_loop"));

        for (int32 i{0}; i < num_cells; ++i) {
            auto const delta_pos{get_position_from_origin_cell_centre(i)};
            auto const pos{origin + delta_pos};

            auto const& potential{cells[i].potential};
            auto const rot_vec{FVector{potential}.Rotation()};

            auto const strength{static_cast<float>(potential.Size())};
            auto const length_scale{FMath::Max(min_length_scale, strength / max_abs_strength)};

            FVector scale{
                length_scale,
                1.f,
                1.f,
            };
            scale *= vector_base_scale;

            vector_transforms.Emplace(rot_vec, pos, scale);

            vector_intensities[i] = strength;
        }
    }

    auto const num_hism_instances{hism.GetInstanceCount()};
    auto const num_new_hism_instances_needed{num_cells - num_hism_instances};

    constexpr bool return_indices{false};
    constexpr bool world_space{true};
    constexpr bool update_nav{false};
    constexpr bool mark_render_dirty{false};
    constexpr bool teleport{true};

    if (num_new_hism_instances_needed > 0) {
        TRACE_CPUPROFILER_EVENT_SCOPE(
            TEXT("ATestUniformField::update_hism_visualisation::dummies"));

        hism.PreAllocateInstancesMemory(num_new_hism_instances_needed);

        TArray<FTransform> dummies;
        dummies.Reserve(num_new_hism_instances_needed);
        for (int32 i{0}; i < num_new_hism_instances_needed; i++) {
            dummies.Add(FTransform::Identity);
        }

        hism.AddInstances(dummies, return_indices, world_space, update_nav);

        UE_LOG(LogSandboxLearning,
               Verbose,
               TEXT("Added %d new HISM instances (total: %d)"),
               num_new_hism_instances_needed,
               hism.GetInstanceCount());

    } else if (num_new_hism_instances_needed < 0) {
        TRACE_CPUPROFILER_EVENT_SCOPE(
            TEXT("ATestUniformField::update_hism_visualisation::hide_loop"));

        auto const num_instances_to_hide{FMath::Abs(num_new_hism_instances_needed)};
        int32 const start_index{num_hism_instances - num_instances_to_hide};
        auto hidden_transform{FTransform::Identity};
        hidden_transform.SetScale3D(FVector::ZeroVector);

        hism.BatchUpdateInstancesTransform(start_index,
                                           num_instances_to_hide,
                                           hidden_transform,
                                           world_space,
                                           mark_render_dirty,
                                           teleport);
    }

    {
        TRACE_CPUPROFILER_EVENT_SCOPE(
            TEXT("ATestUniformField::update_hism_visualisation::batch_update"));
        hism.BatchUpdateInstancesTransforms(
            0, vector_transforms, world_space, mark_render_dirty, teleport);
    }

    {
        TRACE_CPUPROFILER_EVENT_SCOPE(
            TEXT("ATestUniformField::update_hism_visualisation::custom_data"));
        constexpr int32 data_idx{0};
        for (int32 i{0}; i < num_cells; ++i) {
            hism.SetCustomDataValue(i, data_idx, vector_intensities[i], mark_render_dirty);
        }
    }

    hism.MarkRenderStateDirty();
    visualisation_dirty = false;
}

void ATestUniformField::mark_all_dirty() {
    grid_dirty = true;
    visualisation_dirty = true;
}

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
