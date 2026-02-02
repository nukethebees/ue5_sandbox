// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/pathfinding/PatrolWaypoint.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

APatrolWaypoint::APatrolWaypoint()
#if WITH_EDITOR
    : editor_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EditorMesh"))}
#endif
{
    PrimaryActorTick.bCanEverTick = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
#if WITH_EDITOR
    editor_mesh->SetupAttachment(RootComponent);
    editor_mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
#endif
}

void APatrolWaypoint::set_name(FName new_name) {
    if (name.IsNone()) {
        UE_LOG(LogTemp, Warning, TEXT("Tried to set waypoint to None name."));
        return;
    }

    name = new_name;
    SetActorLabel(name.ToString());
}

#if WITH_EDITOR
void APatrolWaypoint::PostEditChangeProperty(FPropertyChangedEvent& event) {
    Super::PostEditChangeProperty(event);

    FName property_name{event.Property ? event.Property->GetFName() : NAME_None};
    if (property_name == GET_MEMBER_NAME_CHECKED(APatrolWaypoint, name)) {
        SetActorLabel(name.ToString());
    }
}
void APatrolWaypoint::OnActorLabelChanged(AActor* actor) {
    if (auto* wp{Cast<ThisClass>(actor)}) {
        wp->name = *wp->GetActorLabel();
    }
}
#endif

void APatrolWaypoint::OnConstruction(FTransform const& Transform) {
    Super::OnConstruction(Transform);

#if WITH_EDITOR
    name = *GetActorLabel();
#endif
}
