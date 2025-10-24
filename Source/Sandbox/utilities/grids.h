#pragma once

#include "CoreMinimal.h"

#include "Sandbox/inventory/data/Coord.h"
#include "Sandbox/inventory/data/Dimensions.h"

namespace ml {
inline auto to_1d_index(FDimensions grid, FCoord coord) -> int32 {
    return coord.row() * grid.width() + coord.column();
}
inline auto to_coord(FDimensions grid, int32 index) -> FCoord {
    return FCoord{index % grid.width(), index / grid.width()};
}
}
