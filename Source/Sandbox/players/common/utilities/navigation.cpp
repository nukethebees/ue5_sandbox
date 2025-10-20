#include "Sandbox/players/common/utilities/navigation.h"

#include "NavigationSystem.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

namespace ml {
auto get_random_nav_point(AActor* actor, float radius) -> std::optional<FNavLocation> {
    RETURN_VALUE_IF_FALSE(actor, {});

    auto* world{actor->GetWorld()};
    RETURN_VALUE_IF_FALSE(world, {});

    auto* nav_sys{UNavigationSystemV1::GetCurrent(world)};
    RETURN_VALUE_IF_FALSE(nav_sys, {});

    auto const actor_location{actor->GetActorLocation()};
    FNavLocation random_point{};

    RETURN_VALUE_IF_FALSE(
        nav_sys->GetRandomReachablePointInRadius(actor_location, radius, random_point), {});

    return random_point;
}
}
