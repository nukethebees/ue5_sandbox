#include "Sandbox/combat/projectiles/actors/Rocket.h"

#include "Components/StaticMeshComponent.h"

ARocket::ARocket()
    : mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"))) {
    mesh->SetupAttachment(RootComponent);
}
