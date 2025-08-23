#include "Sandbox/actors/Coin.h"

#include "Sandbox/actor_components/CoinCollectorActorComponent.h"
#include "Sandbox/characters/MyCharacter.h"

ACoin::ACoin() {
    PrimaryActorTick.bCanEverTick = true;
    SetActorEnableCollision(true);
}

void ACoin::BeginPlay() {
    Super::BeginPlay();
}

void ACoin::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    auto const rotation = FRotator(0.0f, rotation_speed * DeltaTime, 0.0f);
    AddActorLocalRotation(rotation);
}
void ACoin::NotifyActorBeginOverlap(AActor* other_actor) {
    if (auto* const player{Cast<AMyCharacter>(other_actor)}) {
        if (auto* const coin_collector{
                player->FindComponentByClass<UCoinCollectorActorComponent>()}) {
            coin_collector->add_coins(coin_value);
            Destroy();
        }
    }
}
