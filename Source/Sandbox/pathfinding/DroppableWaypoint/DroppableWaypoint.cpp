#include "Sandbox/pathfinding/DroppableWaypoint/DroppableWaypoint.h"

ADroppableWaypoint::ADroppableWaypoint()
    : mesh_component{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"))} {}

void ADroppableWaypoint::Activate() {}
void ADroppableWaypoint::Deactivate() {}
