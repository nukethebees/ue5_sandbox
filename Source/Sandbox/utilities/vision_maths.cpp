#include "Sandbox/utilities/vision_maths.h"

#include "Sandbox/utilities/geometry.h"

#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace ml {
auto get_abs_angle_from_fwd_vector(FVector standing_pt, FVector fwd, AActor const& target)
    -> double {
    auto const to_target{target.GetActorLocation() - standing_pt};
    auto const to_target_norm{to_target.GetSafeNormal()};
    auto const angle_between{ml::get_angle_between_norm_lines(fwd, to_target_norm)};

    return angle_between;
}

auto find_actors_within_cone(AActor const& actor,
                             float const vision_radius,
                             float const capsule_half_height,
                             float const vision_half_angle_rads,
                             TArrayView<TSubclassOf<AActor> const> actor_filter,
                             FCollisionObjectQueryParams object_query_params,
                             FCollisionQueryParams query_params) -> TArray<AActor*> {

    TArray<AActor*> actors;

    auto* world{actor.GetWorld()};
    if (!world) {
        return actors;
    }

    query_params.AddIgnoredActor(&actor);

    TArray<FOverlapResult> overlaps;
    auto const pos{actor.GetActorLocation()};
    auto const shape{FCollisionShape::MakeCapsule(vision_radius, capsule_half_height)};
    if (!world->OverlapMultiByObjectType(
            overlaps, pos, FQuat::Identity, object_query_params, shape, query_params)) {
        return actors;
    }

    auto const fwd{actor.GetActorForwardVector()};

    TArray<AActor*> tmp_actors;
    tmp_actors.Reserve(overlaps.Num());

    // Filter by angle
    for (auto& overlap : overlaps) {
        auto* overlapped_actor{overlap.GetActor()};
        if ((overlapped_actor == nullptr) || (overlapped_actor == &actor)) {
            continue;
        }

        auto const angle{get_abs_angle_from_fwd_vector(pos, fwd, *overlapped_actor)};
        if (angle <= vision_half_angle_rads) {
            tmp_actors.Add(overlapped_actor);
        }
    }

    // Filter by type
    if (actor_filter.Num()) {
        for (auto* other_actor : tmp_actors) {
            for (auto const& f : actor_filter) {
                if (other_actor->IsA(f)) {
                    actors.Add(other_actor);
                    break;
                }
            }
        }
    } else {
        actors = std::move(tmp_actors);
    }

    tmp_actors = std::move(actors);
    actors.Reset();
    actors.Reserve(tmp_actors.Num());

    // Do a final raycast
    for (auto* other_actor : tmp_actors) {
        FHitResult hit;

        auto const blocked{world->LineTraceSingleByChannel(hit,
                                                           pos,
                                                           other_actor->GetActorLocation(),
                                                           ECollisionChannel::ECC_Visibility,
                                                           query_params)};

        if (!blocked || hit.GetActor() == other_actor) {
            actors.Add(other_actor);
        }
    }

    return actors;
}

}
