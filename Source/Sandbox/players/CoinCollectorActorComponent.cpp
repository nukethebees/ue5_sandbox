#include "Sandbox/players/CoinCollectorActorComponent.h"

#include "Sandbox/game_flow/PlatformerGameState.h"

#include "Engine/World.h"

UCoinCollectorActorComponent::UCoinCollectorActorComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UCoinCollectorActorComponent::BeginPlay() {
    Super::BeginPlay();
    coin_count_ = 0;
}

void UCoinCollectorActorComponent::add_coins(int32 number) {
    coin_count_ += number;

    if (auto* game_state{GetWorld()->GetGameState<APlatformerGameState>()}) {
        game_state->notify_coin_change(coin_count_);
    }

    on_coin_count_changed.Broadcast(coin_count_);
}
