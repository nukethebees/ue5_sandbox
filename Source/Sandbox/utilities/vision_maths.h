#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "Templates/SubclassOf.h"

class AActor;

namespace ml {
auto get_abs_angle_from_fwd_vector(FVector standing_pt, FVector fwd, AActor const& target)
    -> double;

auto find_actors_within_cone(AActor const& actor,
                             float const vision_radius,
                             float const capsule_half_height,
                             float const vision_half_angle_rads,
                             TArrayView<TSubclassOf<AActor> const> actor_filter,
                             FCollisionObjectQueryParams object_query_params =
                                 FCollisionObjectQueryParams::DefaultObjectQueryParam,
                             FCollisionQueryParams query_params =
                                 FCollisionQueryParams::DefaultQueryParam) -> TArray<AActor*>;

}
