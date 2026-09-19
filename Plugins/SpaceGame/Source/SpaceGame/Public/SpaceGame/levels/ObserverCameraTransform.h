#pragma once

#include <CoreMinimal.h>

struct FObserverCameraTransform {
    FVector focus{FVector::ZeroVector};
    FTransform transform{FTransform::Identity};
};

namespace ml {
SPACEGAME_API auto make_observer_camera_transform(TConstArrayView<FVector> target_positions,
                                                  FVector offset_direction,
                                                  double distance)
    -> TOptional<FObserverCameraTransform>;
}
