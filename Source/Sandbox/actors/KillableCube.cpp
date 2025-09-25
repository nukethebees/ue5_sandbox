#include "KillableCube.h"

#include "components/StaticMeshComponent.h"
#include "engine/Engine.h"

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
    health_component_->on_death.AddDynamic(this, &AKillableCube::on_death);
}

void AKillableCube::on_death() {
    // Simple death behavior - destroy the actor after a short delay
    SetLifeSpan(0.1f);
}
