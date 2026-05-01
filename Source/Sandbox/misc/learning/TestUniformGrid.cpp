#include "TestUniformGrid.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include "Components/BoxComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Logging/LogMacros.h"
#include "Materials/MaterialInstanceDynamic.h"

auto FScale3D::get() const -> FVector {
    if (use_uniform_scale) {
        return {uniform_scale, uniform_scale, uniform_scale};
    } else {
        return non_uniform_scale;
    }
}

ATestUniformGrid::ATestUniformGrid()
    : volume_box{CreateDefaultSubobject<UBoxComponent>(TEXT("volume_box"))}
    , preview_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("preview_mesh"))}
    , cell_bounds_instances{CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
          TEXT("cell_bounds_instances"))}
    , cell_points_instances{CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
          TEXT("cell_points_instances"))} {

    check(volume_box);
    check(preview_mesh);
    check(cell_bounds_instances);
    check(cell_points_instances);

    RootComponent = volume_box;

    preview_mesh->SetupAttachment(RootComponent);
    cell_bounds_instances->SetupAttachment(RootComponent);
    cell_points_instances->SetupAttachment(RootComponent);

    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bCanEverTick = true;
}
void ATestUniformGrid::BeginPlay() {
    Super::BeginPlay();
}
void ATestUniformGrid::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    UE_LOG(LogSandboxLearning, Display, TEXT("OnConstruction()"));

    create_material_instance();
    update_grid();
}
void ATestUniformGrid::Tick(float dt) {
    Super::Tick(dt);

    if (point_visuals.visible) {
        draw_grid_debug_points();
    }
}
// Grid
void ATestUniformGrid::update_grid() {
    UE_LOG(LogSandboxLearning, Display, TEXT("update_grid()"));

    // Geometry
#if WITH_EDITOR
    update_dimensions();
#endif
    update_grid_coordinates();

    // Visuals
    configure_box();
    configure_preview_mesh();
    configure_cell_ism();

    if (point_visuals.visible) {
        draw_grid_debug_points();
    }

    draw_cell_meshes();
}
void ATestUniformGrid::update_grid_coordinates() {
    auto const nx{FMath::FloorToInt32(box_extent.X / cell_extent.X)};
    auto const ny{FMath::FloorToInt32(box_extent.Y / cell_extent.Y)};
    auto const nz{FMath::FloorToInt32(box_extent.Z / cell_extent.Z)};

    grid_cell_counts = {nx, ny, nz};
}
auto ATestUniformGrid::get_grid_origin() const -> FVector {
    return get_box_origin() + cell_extent;
}
void ATestUniformGrid::draw_grid_debug_points() {
    auto* world{GetWorld()};
    if (!world) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("world is nullptr"));
        return;
    }

    auto const origin{get_grid_origin()};
    auto const counts{grid_cell_counts};
    auto const cell_size{get_cell_dimensions()};

    for (int32 x{0}; x < counts.X; ++x) {
        for (int32 y{0}; y < counts.Y; ++y) {
            for (int32 z{0}; z < counts.Z; ++z) {
                auto const pos{get_cell_position(origin, cell_size, x, y, z)};
                DrawDebugPoint(world, pos, point_visuals.size, point_visuals.colour, false);
                DrawDebugBox(world, pos, cell_extent, point_visuals.colour, false);
            }
        }
    }
}

// Box
auto ATestUniformGrid::get_box_origin() const -> FVector {
    return GetActorLocation() - box_extent;
}
void ATestUniformGrid::create_material_instance() {
    if (!preview_material_parent) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("preview_material_parent is nullptr."));
        return;
    }

    preview_material_instance = UMaterialInstanceDynamic::Create(preview_material_parent, this);
    preview_mesh->SetMaterial(0, preview_material_instance);

    preview_material_instance->SetVectorParameterValue(TEXT("colour"), box_preview_settings.colour);
    preview_material_instance->SetScalarParameterValue(TEXT("opacity_edge_start"),
                                                       box_preview_settings.opacity_edge_start);
}
void ATestUniformGrid::configure_box() {
    volume_box->SetBoxExtent(box_extent);
    volume_box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void ATestUniformGrid::configure_preview_mesh() {
    preview_mesh->SetVisibility(box_preview_settings.visible, box_preview_settings.visible);
    // box_extent is half size
    // Assuming standard 100x100x100 cube
    preview_mesh->SetRelativeScale3D(box_extent * 2.0 / 100.0);
    preview_mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// Cells
auto ATestUniformGrid::get_cell_transform(FVector const& position,
                                          FVector const& mesh_extent,
                                          FVector const& scale_factor) const -> FTransform {
    auto const scale{cell_extent / mesh_extent * scale_factor};

    return FTransform{FRotator::ZeroRotator, position, scale};
}
void ATestUniformGrid::configure_cell_ism(UHierarchicalInstancedStaticMeshComponent& hism) {
    // Collision
    hism.SetCollisionEnabled(ECollisionEnabled::NoCollision);
    hism.SetCollisionResponseToAllChannels(ECR_Ignore);
    hism.SetGenerateOverlapEvents(false);
    hism.SetAllUseCCD(false);
    hism.SetAllUseMACD(false);
    hism.SetEnableGravity(false);

    // Visuals
    hism.SetCastShadow(false);
    hism.bCastDynamicShadow = false;
    hism.bCastStaticShadow = false;
    hism.SetAffectDistanceFieldLighting(false);
    hism.SetAffectDynamicIndirectLighting(false);
    hism.SetAffectIndirectLightingWhileHidden(false);

    hism.SetCanEverAffectNavigation(false);
}
void ATestUniformGrid::configure_cell_ism() {
    configure_cell_ism(*cell_bounds_instances);
    configure_cell_ism(*cell_points_instances);
}
void ATestUniformGrid::draw_cell_meshes() {
    UE_LOG(LogSandboxLearning, Display, TEXT("draw_cell_meshes()"));

    cell_bounds_instances->SetVisibility(cell_bounds_preview_settings.visible);
    cell_points_instances->SetVisibility(cell_points_preview_settings.visible);

    auto const bounds_mesh{cell_bounds_instances->GetStaticMesh()};
    if (!bounds_mesh) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("cell_bounds_instances has no static mesh."))
        return;
    }
    auto const point_mesh{cell_points_instances->GetStaticMesh()};
    if (!point_mesh) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("cell_points_instances has no static mesh."))
        return;
    }

    auto const origin{get_grid_origin()};
    auto const counts{grid_cell_counts};
    auto const cell_size{get_cell_dimensions()};
    auto const bounds_mesh_bounds{bounds_mesh->GetBounds()};
    auto const points_mesh_bounds{point_mesh->GetBounds()};

    auto const num_cells{get_num_cells()};
    TArray<FTransform> bounds_transforms;
    bounds_transforms.Reserve(num_cells);
    TArray<FTransform> points_transforms;
    points_transforms.Reserve(num_cells);

    auto const bounds_scale_factor{cell_bounds_preview_settings.get_scale()};
    auto const points_scale_factor{cell_points_preview_settings.get_scale()};

    for (int32 x{0}; x < counts.X; ++x) {
        for (int32 y{0}; y < counts.Y; ++y) {
            for (int32 z{0}; z < counts.Z; ++z) {
                auto const pos{get_cell_position(origin, cell_size, x, y, z)};
                auto const bounds_transform{
                    get_cell_transform(pos, bounds_mesh_bounds.BoxExtent, bounds_scale_factor)};
                bounds_transforms.Emplace(bounds_transform);

                auto const point_transform{
                    get_cell_transform(pos, points_mesh_bounds.BoxExtent, points_scale_factor)};
                points_transforms.Emplace(point_transform);
            }
        }
    }

    // Get current instance count
    auto const num_hism_instances{cell_bounds_instances->GetInstanceCount()};
    auto const num_new_hism_instances_needed{num_cells - num_hism_instances};

    UE_LOG(LogSandboxLearning, Verbose, TEXT("num_cells: %d"), num_cells);
    UE_LOG(LogSandboxLearning, Verbose, TEXT("num_hism_instances: %d"), num_hism_instances);
    UE_LOG(LogSandboxLearning,
           Verbose,
           TEXT("num_new_hism_instances_needed: %d"),
           num_new_hism_instances_needed);

    constexpr bool return_indices{false};
    constexpr bool world_space{true};
    constexpr bool update_nav{false};
    constexpr bool mark_render_dirty{false};
    constexpr bool teleport{true};

    if (num_new_hism_instances_needed > 0) {
        cell_bounds_instances->PreAllocateInstancesMemory(num_new_hism_instances_needed);
        cell_points_instances->PreAllocateInstancesMemory(num_new_hism_instances_needed);

        TArray<FTransform> dummies;
        dummies.Reserve(num_new_hism_instances_needed);
        for (int32 i{0}; i < num_new_hism_instances_needed; i++) {
            dummies.Add(FTransform::Identity);
        }

        cell_bounds_instances->AddInstances(dummies, return_indices, world_space, update_nav);
        cell_points_instances->AddInstances(dummies, return_indices, world_space, update_nav);
    } else if (num_new_hism_instances_needed < 0) {
        auto const num_instances_to_hide{FMath::Abs(num_new_hism_instances_needed)};

        auto hidden_transform{FTransform::Identity};
        hidden_transform.SetScale3D(FVector::ZeroVector);

        int32 const start_index{num_hism_instances - num_instances_to_hide};

        UE_LOG(LogSandboxLearning,
               Verbose,
               TEXT("Hiding %d instances from index %d"),
               num_instances_to_hide,
               start_index);

        cell_bounds_instances->BatchUpdateInstancesTransform(start_index,
                                                             num_instances_to_hide,
                                                             hidden_transform,
                                                             world_space,
                                                             mark_render_dirty,
                                                             teleport);
        cell_points_instances->BatchUpdateInstancesTransform(start_index,
                                                             num_instances_to_hide,
                                                             hidden_transform,
                                                             world_space,
                                                             mark_render_dirty,
                                                             teleport);
    }

    UE_LOG(LogSandboxLearning,
           Verbose,
           TEXT("Updating %d transforms for %d cells"),
           bounds_transforms.Num(),
           num_cells);

    cell_bounds_instances->BatchUpdateInstancesTransforms(
        0, bounds_transforms, world_space, mark_render_dirty, teleport);
    cell_points_instances->BatchUpdateInstancesTransforms(
        0, points_transforms, world_space, mark_render_dirty, teleport);

    cell_bounds_instances->MarkRenderStateDirty();
    cell_points_instances->MarkRenderStateDirty();
}

#if WITH_EDITOR
void ATestUniformGrid::PostEditChangeProperty(FPropertyChangedEvent& event) {
    Super::PostEditChangeProperty(event);

    FName const property_name{event.Property ? event.Property->GetFName() : NAME_None};

    if (property_name == GET_MEMBER_NAME_CHECKED(ATestUniformGrid, box_extent)) {
        update_grid();
    } else if (property_name == GET_MEMBER_NAME_CHECKED(ATestUniformGrid, cell_extent)) {
        update_grid();
    }
}

void ATestUniformGrid::update_dimensions() {
    num_cells_ = get_num_cells();
    box_dimensions_ = get_box_dimensions();
    cell_dimensions_ = get_cell_dimensions();
}
#endif
