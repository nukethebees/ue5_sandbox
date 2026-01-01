#include "Sandbox/players/npcs/actor_components/NpcPatrolComponent.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/pathfinding/PatrolPath/PatrolPath.h"
#include "Sandbox/pathfinding/PatrolPath/PatrolWaypoint.h"

UNpcPatrolComponent::UNpcPatrolComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UNpcPatrolComponent::BeginPlay() {
    Super::BeginPlay();
}

auto UNpcPatrolComponent::get_current_point() -> APatrolWaypoint* {
    return current_waypoint;
}
auto UNpcPatrolComponent::get_current_point() const -> APatrolWaypoint* {
    return current_waypoint;
}
auto UNpcPatrolComponent::get_previous_point() -> APatrolWaypoint* {
    return last_waypoint;
}
auto UNpcPatrolComponent::get_previous_point() const -> APatrolWaypoint* {
    return last_waypoint;
}

void UNpcPatrolComponent::advance_to_next_point() {
    check(patrol_path);

    auto& path{*patrol_path};

    if (!last_waypoint) {
        set_waypoint_to_default();
        return;
    }

    int32 i{0};
    int32 const n{path.length()};
    bool found{false};

    for (; i < n; i++) {
        auto* wp{path[i]};
        if (wp == current_waypoint) {
            found = true;
            break;
        }
    }

    if (!found) {
        UE_LOG(LogSandboxAI, Warning, TEXT("Couldn't find the existing point."));
        set_waypoint_to_default();
        return;
    }

    auto const next_index{i + 1};
    if (next_index >= n) {
        current_waypoint = path[0];
    } else {
        current_waypoint = path[i + 1];
    }

    if (!current_waypoint) {
        UE_LOG(LogSandboxAI, Warning, TEXT("Couldn't find the next waypoint."));
        set_waypoint_to_default();
    }
}
void UNpcPatrolComponent::set_arrived_at_point() {
    last_waypoint = current_waypoint;
}
auto UNpcPatrolComponent::arrived_at_point() const -> bool {
    return last_waypoint == current_waypoint;
}
void UNpcPatrolComponent::set_current_waypoint(APatrolWaypoint& wp) {
    last_waypoint = current_waypoint;
    current_waypoint = &wp;
}
void UNpcPatrolComponent::set_waypoint_to_default() {
    check(patrol_path);
    auto& path{*patrol_path};
    auto& parent{*GetOwner()};

    auto const n{path.length()};

    if (n < 2) {
        if (auto const point{path.get_first_point()}) {
            set_current_waypoint(*point);
        } else {
            return;
        }
    }

    auto* point{path[0]};
    auto min_dist{parent.GetDistanceTo(point)};

    for (int32 i{1}; i < n; i++) {
        auto* new_point{path[i]};
        auto const dist{parent.GetDistanceTo(new_point)};
        if (dist < min_dist) {
            point = new_point;
            min_dist = dist;
        }
    }

    return set_current_waypoint(*point);
}
