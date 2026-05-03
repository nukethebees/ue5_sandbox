#include "TestUniformField.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"

#include <algorithm>
#include <limits>

void FTestUniformFieldCell::reset() {
    potential = FVector::ZeroVector;
}

ATestUniformField::ATestUniformField()
    : vector_meshes{CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
          TEXT("vector_meshes"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    vector_meshes->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    // Run when everyone else is finished
    PrimaryActorTick.TickGroup = ETickingGroup::TG_PostPhysics;
}

void ATestUniformField::BeginPlay() {
    Super::BeginPlay();
}
void ATestUniformField::Tick(float dt) {
    Super::Tick(dt);

#if WITH_EDITOR
    dbg_log_timer += dt;
#endif

    reset_cells();
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
    configure_visualisation_component();
    update_visualisation();
}

void ATestUniformField::add_source(FTestUniformFieldPointSourceData const& source) {
    point_sources.Add(source);
}

void ATestUniformField::construct_grid() {
    auto const n{get_num_cells()};
    cells.Reset();
    cells.AddDefaulted(n);
}
void ATestUniformField::update_cells() {
    auto const n_cells{get_num_cells()};
    auto const origin{get_origin_cell_centre()};

    reset_cells();

    for (auto const& ps : point_sources) {
        auto const source_pos{ps.coordinate};

        for (int32 i{0}; i < n_cells; ++i) {
            auto const delta_pos{get_position_from_origin_cell_centre(i)};
            auto const cell_pos{origin + delta_pos};
            auto const displacement{cell_pos - source_pos};
            auto const dist_cm{static_cast<float>(displacement.Length())};
            auto const dist_m{dist_cm / 100.f};

            auto const strength{ps.strength * FMath::Pow(dist_m, ps.falloff)};
            auto const potential{displacement.GetSafeNormal() * strength};

            max_strength = std::max(max_strength, strength);
            min_strength = std::min(min_strength, strength);
            auto const abs_strength{std::abs(strength)};
            max_abs_strength = std::max(max_abs_strength, abs_strength);
            min_abs_strength = std::min(min_abs_strength, abs_strength);

            auto& cell{cells[i]};
            cell.potential += potential;
        }
    }

    point_sources.Reset();
}
void ATestUniformField::reset_cells() {
    for (auto& cell : cells) {
        cell.reset();
    }

    constexpr auto inf{std::numeric_limits<float>::infinity()};

    max_strength = -inf;
    min_strength = inf;
    min_abs_strength = inf;
    max_abs_strength = 0.f;
}
void ATestUniformField::configure_visualisation_component() {
    auto& hism{*vector_meshes};

    // Collision
    hism.SetCollisionEnabled(ECollisionEnabled::NoCollision);
    hism.SetCollisionResponseToAllChannels(ECR_Ignore);
    hism.SetGenerateOverlapEvents(false);
    hism.SetAllUseCCD(false);
    hism.SetAllUseMACD(false);
    hism.SetEnableGravity(false);

    // Visuals
    // hism.NumCustomDataFloats = 1;
    hism.SetNumCustomDataFloats(1);

    hism.SetCastShadow(false);
    hism.bCastDynamicShadow = false;
    hism.bCastStaticShadow = false;
    hism.SetAffectDistanceFieldLighting(false);
    hism.SetAffectDynamicIndirectLighting(false);
    hism.SetAffectIndirectLightingWhileHidden(false);

    hism.SetCanEverAffectNavigation(false);
}
void ATestUniformField::update_visualisation() {
    auto& hism{*vector_meshes};

    hism.SetVisibility(display_vectors);

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

    for (int32 i{0}; i < num_cells; ++i) {
        auto const delta_pos{get_position_from_origin_cell_centre(i)};
        auto const pos{origin + delta_pos};

        auto const& potential{cells[i].potential};
        auto const rot_vec{potential.Rotation()};

        vector_transforms.Emplace(rot_vec, pos, FVector::OneVector);

        auto const potential_len{static_cast<float>(potential.Size())};
        vector_intensities[i] = potential_len / max_abs_strength;
    }

    auto const num_hism_instances{hism.GetInstanceCount()};
    auto const num_new_hism_instances_needed{num_cells - num_hism_instances};

    constexpr bool return_indices{false};
    constexpr bool world_space{true};
    constexpr bool update_nav{false};
    constexpr bool mark_render_dirty{false};
    constexpr bool teleport{true};

    if (num_new_hism_instances_needed > 0) {
        hism.PreAllocateInstancesMemory(num_new_hism_instances_needed);

        TArray<FTransform> dummies;
        dummies.Reserve(num_new_hism_instances_needed);
        for (int32 i{0}; i < num_new_hism_instances_needed; i++) {
            dummies.Add(FTransform::Identity);
        }

        hism.AddInstances(dummies, return_indices, world_space, update_nav);
    } else if (num_new_hism_instances_needed < 0) {
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

    hism.BatchUpdateInstancesTransforms(
        0, vector_transforms, world_space, mark_render_dirty, teleport);

    constexpr int32 data_idx{0};
    for (int32 i{0}; i < num_cells; ++i) {
        hism.SetCustomDataValue(i, data_idx, vector_intensities[i], mark_render_dirty);
    }

    hism.MarkRenderStateDirty();
}

auto ATestUniformField::get_coord(FVector const& pos) const -> FIntVector {
    return {};
}
auto ATestUniformField::get_index(FIntVector const& pos) const -> int32 {
    return {};
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

    return loc - get_grid_extent();
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

auto ATestUniformField::get_grid_extent() const -> FVector {
    return cell_extent * FVector{grid_dimensions};
}
auto ATestUniformField::get_grid_dimensions() const -> FVector {
    return get_grid_extent() * 2.0;
}
