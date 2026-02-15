#include "Sandbox/players/navigation.h"

#include "NavigationSystem.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

namespace ml {
auto get_random_nav_point(AActor& actor, float radius) -> std::optional<FNavLocation> {
    INIT_PTR_OR_RETURN_VALUE(world, actor.GetWorld(), {});
    INIT_PTR_OR_RETURN_VALUE(nav_sys, UNavigationSystemV1::GetCurrent(world), {});

    auto const actor_location{actor.GetActorLocation()};
    FNavLocation random_point{};

    RETURN_VALUE_IF_FALSE(
        nav_sys->GetRandomReachablePointInRadius(actor_location, radius, random_point), {});

    return random_point;
}
}
