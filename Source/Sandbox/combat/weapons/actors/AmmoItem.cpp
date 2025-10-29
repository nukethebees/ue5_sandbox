#include "Sandbox/combat/weapons/actors/AmmoItem.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Sandbox/constants/collision_channels.h"

AAmmoItem::AAmmoItem()
    : collision_box{CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"))}
    , mesh_component{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    collision_box->SetupAttachment(RootComponent);
    collision_box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    collision_box->SetCollisionResponseToAllChannels(ECR_Ignore);
    collision_box->SetCollisionResponseToChannel(ml::collision::interaction, ECR_Block);
    collision_box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    mesh_component->SetupAttachment(RootComponent);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AAmmoItem::BeginPlay() {
    Super::BeginPlay();
}
