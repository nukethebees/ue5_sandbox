#include "Sandbox/combat/weapons/actors/RocketLauncher.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ARocketLauncher::ARocketLauncher()
    : mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"))) {
    mesh->SetupAttachment(RootComponent);
}
