#include "Sandbox/actor_components/JetpackComponent.h"

UJetpackComponent::UJetpackComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}
void UJetpackComponent::BeginPlay() {
    Super::BeginPlay();
}
