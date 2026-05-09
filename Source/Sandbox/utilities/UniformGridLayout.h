#pragma once

#include "CoreMinimal.h"

#include "UniformGridLayout.generated.h"

UENUM()
enum class EUniformGridLayoutEditMode : uint8 {
    DimAndSpatialExtent,
    DimAndCellExtent,
    SpatialAndCellExtent
};

USTRUCT()
struct FUniformGridLayout {
    GENERATED_BODY()

    FUniformGridLayout() = default;

    void init();

    auto get_num_cells() const -> int32;
    auto get_grid_coords_sum() const -> int32;

    auto get_centre() const -> FVector;
    auto get_origin() const -> FVector;
    auto get_origin_cell_centre() const -> FVector;

    auto get_spatial_extent() const -> FVector;
    auto get_spatial_dimensions() const -> FVector;

    auto get_cell_extent() const -> FVector;
    auto get_cell_dimensions() const -> FVector;

    UPROPERTY(EditAnywhere, Category = "Grid")
    EUniformGridLayoutEditMode edit_mode{EUniformGridLayoutEditMode::SpatialAndCellExtent};
    UPROPERTY(
        EditAnywhere,
        Category = "Grid",
        meta = (EditCondition = "edit_mode != EUniformGridLayoutEditMode::SpatialAndCellExtent"))
    FIntVector coordinate_dimensions{1, 1, 1};
    UPROPERTY(EditAnywhere,
              Category = "Grid",
              meta = (EditCondition = "edit_mode != EUniformGridLayoutEditMode::DimAndCellExtent"))
    FVector spatial_extent{500.f};
    UPROPERTY(
        EditAnywhere,
        Category = "Grid",
        meta = (EditCondition = "edit_mode != EUniformGridLayoutEditMode::DimAndSpatialExtent"))
    FVector cell_extent{50.f};
};
