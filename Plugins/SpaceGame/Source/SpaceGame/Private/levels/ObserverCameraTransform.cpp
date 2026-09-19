#include "SpaceGame/levels/ObserverCameraTransform.h"

namespace ml {
auto make_observer_camera_transform(TConstArrayView<FVector> const target_positions,
                                    FVector const offset_direction,
                                    double const distance) -> TOptional<FObserverCameraTransform> {
    if (target_positions.IsEmpty() || offset_direction.ContainsNaN() ||
        offset_direction.IsNearlyZero() || !FMath::IsFinite(distance) || distance <= 0.0) {
        return NullOpt;
    }

    FVector focus{FVector::ZeroVector};
    for (auto const position : target_positions) {
        if (position.ContainsNaN()) {
            return NullOpt;
        }
        focus += position;
    }
    focus /= target_positions.Num();

    auto const position{focus + offset_direction.GetSafeNormal() * distance};
    return FObserverCameraTransform{
        .focus = focus,
        .transform = FTransform{(focus - position).Rotation(), position},
    };
}
}
