#include "Sandbox/pathfinding/DroppableWaypoint/DroppableWaypoint.h"

#include "Sandbox/environment/effects/actor_components/RotatingActorComponent.h"

ADroppableWaypoint::ADroppableWaypoint()
    : mesh_component{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"))}
    , rotation_component{
          CreateDefaultSubobject<URotatingActorComponent>(TEXT("RotationComponent"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    mesh_component->SetupAttachment(RootComponent);

    rotation_component->rotation_type = ERotationType::STATIC;
}

void ADroppableWaypoint::Activate() {}
void ADroppableWaypoint::Deactivate() {}
