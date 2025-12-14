#include "Sandbox/players/npcs/actor_components/NpcPatrolComponent.h"

UNpcPatrolComponent::UNpcPatrolComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UNpcPatrolComponent::BeginPlay() {
    Super::BeginPlay();
}
