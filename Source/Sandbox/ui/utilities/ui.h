#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "Sandbox/environment/data/ActorCorners.h"
#include "Sandbox/inventory/data/Coord.h"
#include "Sandbox/inventory/data/Dimensions.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/ui/data/ScreenBounds.h"

class AActor;
class APlayerController;
class UGridPanel;

namespace ml {
int32 scale_font_to_umg(int32 slate_dpi);

namespace detail {
using UpdateBoundsFn = void (*)(APlayerController&, FVector, FVector2D&);
}

template <detail::UpdateBoundsFn update_bounds>
auto get_screen_bounds(APlayerController& pc, FActorCorners const& corners) -> FScreenBounds {
    auto corner_view{TArrayView<FVector const>(corners.data)};

    FVector2D top_left{1e9, 1e9};
    FVector2D bottom_right{0, 0};
    FVector2D screen_location{};

    for (auto const& corner : corners.data) {
        update_bounds(pc, corner, screen_location);

        top_left.X = std::min(top_left.X, screen_location.X);
        top_left.Y = std::min(top_left.Y, screen_location.Y);

        bottom_right.X = std::max(bottom_right.X, screen_location.X);
        bottom_right.Y = std::max(bottom_right.Y, screen_location.Y);
    }

    return {top_left, bottom_right - top_left};
}

// If there are no cells it will default to (-1, -1)
auto get_grid_outer_coords(UGridPanel const& grid) -> FCoord;
}
