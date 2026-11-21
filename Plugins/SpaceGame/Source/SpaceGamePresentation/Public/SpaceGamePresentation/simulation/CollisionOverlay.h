#pragma once

#include <ioj/sim/collision/collision_grid_spec.h>

#include <CoreMinimal.h>

class FPrimitiveDrawInterface;

namespace ioj {
struct FCollisionOverlayVisibility {
    bool grid{};
    bool bounds{};
};

struct FCollisionGridLine {
    FVector start;
    FVector end;
};

SPACEGAMEPRESENTATION_API auto make_collision_grid_lines(sim::collision::CollisionGridSpec grid)
    -> TArray<FCollisionGridLine>;
SPACEGAMEPRESENTATION_API void draw_collision_grid_lines(FPrimitiveDrawInterface& pdi,
                                                         TConstArrayView<FCollisionGridLine> lines,
                                                         FLinearColor colour,
                                                         float thickness,
                                                         uint8 depth_priority);
}
