#pragma once

#include "CoreMinimal.h"

class AActor;

namespace ml {
auto get_abs_angle_from_fwd_vector(FVector standing_pt, FVector fwd, AActor const& target)
    -> double;
}
