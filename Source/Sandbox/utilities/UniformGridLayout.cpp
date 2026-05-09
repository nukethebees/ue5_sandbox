#include "UniformGridLayout.h"

void FUniformGridLayout::init() {
    switch (edit_mode) {
        case EUniformGridLayoutEditMode::SpatialAndCellExtent: {
            coordinate_dimensions = FIntVector{spatial_extent / cell_extent};

            break;
        }
        case EUniformGridLayoutEditMode::DimAndSpatialExtent: {
            cell_extent = {
                spatial_extent.X / coordinate_dimensions.X,
                spatial_extent.Y / coordinate_dimensions.Y,
                spatial_extent.Z / coordinate_dimensions.Z,
            };

            break;
        }
        case EUniformGridLayoutEditMode::DimAndCellExtent: {
            spatial_extent = {
                coordinate_dimensions.X * cell_extent.X,
                coordinate_dimensions.Y * cell_extent.Y,
                coordinate_dimensions.Z * cell_extent.Z,
            };

            break;
        }
    }
}

auto FUniformGridLayout::get_num_cells() const -> int32 {
    return coordinate_dimensions.X * coordinate_dimensions.Y * coordinate_dimensions.Z;
}
auto FUniformGridLayout::get_grid_coords_sum() const -> int32 {
    return coordinate_dimensions.X + coordinate_dimensions.Y + coordinate_dimensions.Z;
}

auto FUniformGridLayout::get_centre() const -> FVector {
    return FVector::ZeroVector;
}
auto FUniformGridLayout::get_origin() const -> FVector {
    auto const loc{get_centre()};

    return loc - get_spatial_extent();
}
auto FUniformGridLayout::get_origin_cell_centre() const -> FVector {
    return get_origin() + get_cell_extent();
}

auto FUniformGridLayout::get_spatial_extent() const -> FVector {
    return {
        spatial_extent.X * coordinate_dimensions.X,
        spatial_extent.Y * coordinate_dimensions.Y,
        spatial_extent.Z * coordinate_dimensions.Z,
    };
}
auto FUniformGridLayout::get_spatial_dimensions() const -> FVector {
    return 2.0 * get_spatial_extent();
}

auto FUniformGridLayout::get_cell_extent() const -> FVector {
    return cell_extent;
}
auto FUniformGridLayout::get_cell_dimensions() const -> FVector {
    return cell_extent * 2.0;
}
