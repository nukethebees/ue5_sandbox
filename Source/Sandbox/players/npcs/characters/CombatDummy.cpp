#include "Sandbox/players/npcs/characters/CombatDummy.h"

#include "Components/StaticMeshComponent.h"

#include "Sandbox/health/actor_components/HealthComponent.h"

ACombatDummy::ACombatDummy()
    : body_mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh")))
    , health(CreateDefaultSubobject<UHealthComponent>(TEXT("Health"))) {
    PrimaryActorTick.bCanEverTick = false;

    body_mesh->SetupAttachment(RootComponent);

    health->max_health = 1e9;
    health->initial_health = 1e9;

    AutoPossessAI = EAutoPossessAI::PlacedInWorld;
}

void ACombatDummy::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
}
void ACombatDummy::BeginPlay() {
    Super::BeginPlay();
}

FGenericTeamId ACombatDummy::GetGenericTeamId() const {
    return FGenericTeamId(static_cast<uint8>(team_id));
}
void ACombatDummy::SetGenericTeamId(FGenericTeamId const& TeamID) {
    team_id = static_cast<ETeamID>(TeamID.GetId());
}
