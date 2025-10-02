#include "Sandbox/actors/Coin.h"

#include "Sandbox/actor_components/CoinCollectorActorComponent.h"
#include "Sandbox/characters/MyCharacter.h"
#include "Sandbox/subsystems/world/CollisionEffectSubsystem.h"
#include "Sandbox/subsystems/world/RotationManagerSubsystem.h"

ACoin::ACoin() {
    PrimaryActorTick.bCanEverTick = false;

    constexpr auto mobility{EComponentMobility::Movable};

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(mobility);

    mesh_component = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    mesh_component->SetupAttachment(RootComponent);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    mesh_component->SetMobility(mobility);

    collision_component = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
    collision_component->SetupAttachment(RootComponent);
    collision_component->SetMobility(mobility);
}

void ACoin::BeginPlay() {
    Super::BeginPlay();

    auto* world{GetWorld()};
    if (!world) {
        return;
    }

    if (auto* manager{world->GetSubsystem<URotationManagerSubsystem>()};
        manager && mesh_component) {
        manager->add(*mesh_component, rotation_speed);
    } else {
        UE_LOGFMT(LogTemp, Warning, "Couldn't get URotationManagerSubsystem.");
    }

    if (auto* manager{world->GetSubsystem<UCollisionEffectSubsystem>()}) {
        manager->add_payload(this, FCoinPayload(coin_value));
    }
}
