#include "Sandbox/actors/Coin.h"

#include "Sandbox/actor_components/CoinCollectorActorComponent.h"
#include "Sandbox/characters/MyCharacter.h"
#include "Sandbox/subsystems/RotationManagerSubsystem.h"

ACoin::ACoin() {
    PrimaryActorTick.bCanEverTick = false;
    SetActorEnableCollision(true);
}

void ACoin::BeginPlay() {
    Super::BeginPlay();

    auto* world{GetWorld()};
    if (!world) {
        return;
    }

    if (auto* rotation_manager{world->GetSubsystem<URotationManagerSubsystem>()}) {
        rotation_manager->register_static_rotating_actor(rotation_speed, this);
    } else {
        UE_LOGFMT(LogTemp, Warning, "Couldn't get URotationManagerSubsystem.");
    }
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
