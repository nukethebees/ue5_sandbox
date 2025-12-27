#include "Sandbox/pathfinding/DroppableWaypoint/DroppableWaypoint.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Sandbox/environment/effects/actor_components/RotatingActorComponent.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

ADroppableWaypoint::ADroppableWaypoint()
    : mesh_component{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"))}
    , rotation_component{
          CreateDefaultSubobject<URotatingActorComponent>(TEXT("RotationComponent"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    mesh_component->SetupAttachment(RootComponent);

    rotation_component->rotation_type = ERotationType::STATIC;
}
void ADroppableWaypoint::BeginPlay() {
    Super::BeginPlay();

    Activate();
}
void ADroppableWaypoint::Activate() {
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);

    RETURN_IF_NULLPTR(mesh_component);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    mesh_component->SetCollisionObjectType(ECC_Visibility);
}
void ADroppableWaypoint::Deactivate() {
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    SetActorLocation(FVector::ZeroVector);
}
