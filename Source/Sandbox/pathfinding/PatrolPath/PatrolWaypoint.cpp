// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/pathfinding/PatrolPath/PatrolWaypoint.h"

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
#endif
}

#if WITH_EDITOR
void APatrolWaypoint::PostEditChangeProperty(FPropertyChangedEvent& event) {
    Super::PostEditChangeProperty(event);

    FName property_name{event.Property ? event.Property->GetFName() : NAME_None};
    if (property_name == GET_MEMBER_NAME_CHECKED(APatrolWaypoint, name)) {
        SetActorLabel(name.ToString());
    }
}
#endif
