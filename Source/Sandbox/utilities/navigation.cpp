#include "Sandbox/utilities/navigation.h"

#include "NavigationSystem.h"

namespace ml {
auto get_random_nav_point(AActor* actor, float radius) -> std::optional<FNavLocation> {
    if (!actor) {
        return {};
    }

    auto* world{actor->GetWorld()};
    if (!world) {
        return {};
    }

    auto* nav_sys{UNavigationSystemV1::GetCurrent(world)};
    if (!nav_sys) {
        return {};
    }

    auto const actor_location{actor->GetActorLocation()};
    FNavLocation random_point{};

    if (!nav_sys->GetRandomReachablePointInRadius(actor_location, radius, random_point)) {
        return {};
    }

    return random_point;
}
}
