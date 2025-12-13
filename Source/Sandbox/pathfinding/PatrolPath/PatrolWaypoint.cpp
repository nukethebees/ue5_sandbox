// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/pathfinding/PatrolPath/PatrolWaypoint.h"

APatrolWaypoint::APatrolWaypoint() {
    PrimaryActorTick.bCanEverTick = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
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
