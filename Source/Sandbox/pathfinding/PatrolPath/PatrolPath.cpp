// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/pathfinding/PatrolPath/PatrolPath.h"

APatrolPath::APatrolPath()
{
    PrimaryActorTick.bCanEverTick = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

#if WITH_EDITOR
void APatrolPath::PostEditChangeProperty(FPropertyChangedEvent& event) {
    Super::PostEditChangeProperty(event);

    FName property_name{event.Property ? event.Property->GetFName() : NAME_None};
    if (property_name == GET_MEMBER_NAME_CHECKED(APatrolPath, name)) {
        SetActorLabel(name.ToString());
    }
}
#endif
