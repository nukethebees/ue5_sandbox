#include "KillableCube.h"

#include "components/StaticMeshComponent.h"
#include "engine/Engine.h"

#include "Sandbox/health/actor_components/HealthComponent.h"

AKillableCube::AKillableCube() {
    PrimaryActorTick.bCanEverTick = false;

    // Create root component
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

    // Create and set up mesh component
    mesh_component_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    mesh_component_->SetupAttachment(RootComponent);

    // Create health component
    health_component_ = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
    health_component_->max_health = 100.0f;
}

void AKillableCube::handle_death() {
    SetLifeSpan(0.1f);
}
