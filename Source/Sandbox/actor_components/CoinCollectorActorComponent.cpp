#include "Sandbox/actor_components/CoinCollectorActorComponent.h"

UCoinCollectorActorComponent::UCoinCollectorActorComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UCoinCollectorActorComponent::BeginPlay() {
    Super::BeginPlay();
    coin_count_ = 0;
}
