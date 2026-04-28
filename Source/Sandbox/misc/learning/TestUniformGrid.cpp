#include "TestUniformGrid.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Logging/LogMacros.h"
#include "Materials/MaterialInstanceDynamic.h"

ATestUniformGrid::ATestUniformGrid()
    : volume_box{CreateDefaultSubobject<UBoxComponent>(TEXT("volume_box"))}
    , preview_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("preview_mesh"))} {

    check(volume_box);
    check(preview_mesh);

    RootComponent = volume_box;

    preview_mesh->SetupAttachment(RootComponent);

    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bCanEverTick = true;
}
void ATestUniformGrid::BeginPlay() {
    Super::BeginPlay();
}
void ATestUniformGrid::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    create_material_instance();
    update_grid();
}
void ATestUniformGrid::Tick(float dt) {
    Super::Tick(dt);

    draw_grid_points();
}

void ATestUniformGrid::create_material_instance() {
    if (!preview_material_parent) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("preview_material_parent is nullptr."));
        return;
    }

    preview_material_instance = UMaterialInstanceDynamic::Create(preview_material_parent, this);
    preview_mesh->SetMaterial(0, preview_material_instance);

    preview_material_instance->SetVectorParameterValue(TEXT("colour"),
                                                       preview_material_settings.colour);
    preview_material_instance->SetScalarParameterValue(
        TEXT("opacity_edge_start"), preview_material_settings.opacity_edge_start);
}
void ATestUniformGrid::update_grid() {
    volume_box->SetBoxExtent(box_extent);
    configure_preview_mesh();
#if WITH_EDITOR
    update_dimensions();
#endif
    update_grid_coordinates();
    draw_grid_points();
}
void ATestUniformGrid::update_grid_coordinates() {
    auto const nx{FMath::FloorToInt32(box_extent.X / cell_extent.X)};
    auto const ny{FMath::FloorToInt32(box_extent.Y / cell_extent.Y)};
    auto const nz{FMath::FloorToInt32(box_extent.Z / cell_extent.Z)};

    grid_cell_counts = {nx, ny, nz};
}
void ATestUniformGrid::draw_grid_points() {
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
                FVector const cell_scale{
                    static_cast<double>(x),
                    static_cast<double>(y),
                    static_cast<double>(z),
                };
                auto const offset{cell_size * cell_scale};
                auto const pos{origin + offset};

                DrawDebugPoint(world, pos, point_visuals.size, point_visuals.colour, false);
                DrawDebugBox(world, pos, cell_extent, point_visuals.colour, false);
            }
        }
    }
}
auto ATestUniformGrid::get_box_origin() const -> FVector {
    return GetActorLocation() - box_extent;
}
auto ATestUniformGrid::get_grid_origin() const -> FVector {
    return get_box_origin() + cell_extent;
}
void ATestUniformGrid::configure_preview_mesh() {
    preview_mesh->SetVisibility(show_preview, true);
    // box_extent is half size
    // Assuming standard 100x100x100 cube
    preview_mesh->SetRelativeScale3D(box_extent * 2.0 / 100.0);
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
    box_dimensions = get_box_dimensions();
    cell_dimensions = get_cell_dimensions();
}
#endif
