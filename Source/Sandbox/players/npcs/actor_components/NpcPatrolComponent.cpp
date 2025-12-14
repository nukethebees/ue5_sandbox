#include "Sandbox/players/npcs/actor_components/NpcPatrolComponent.h"

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

    if (!current_waypoint) {
        set_waypoint_to_default();
        return;
    }

    int32 i{0};
    int32 const n{patrol_path->length()};
    bool found{false};

    for (; i < n; i++) {
        auto* wp{(*patrol_path)[i]};
        if (wp == current_waypoint) {
            found = true;
            break;
        }
    }

    if (!found) {
        set_waypoint_to_default();
        return;
    }

    current_waypoint = (*patrol_path)[i + 1];
    if (!current_waypoint) {
        set_waypoint_to_default();
    }
}
void UNpcPatrolComponent::set_arrived_at_point() {
    last_waypoint = current_waypoint;
}
auto UNpcPatrolComponent::arrived_at_point() const -> bool {
    return last_waypoint == current_waypoint;
}
void UNpcPatrolComponent::set_current_waypoint(APatrolWaypoint* wp) {
    last_waypoint = current_waypoint;
    current_waypoint = wp;
}
void UNpcPatrolComponent::set_waypoint_to_default() {
    check(patrol_path);
    set_current_waypoint(patrol_path->get_first_point());
}
