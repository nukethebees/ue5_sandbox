#include "Sandbox/players/npcs/CombatDummy.h"

#include "AIController.h"
#include "Components/StaticMeshComponent.h"

#include "Sandbox/health/HealthComponent.h"
#include "Sandbox/players/npcs/NpcPatrolComponent.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

ACombatDummy::ACombatDummy()
    : body_mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh")))
    , patrol_state(CreateDefaultSubobject<UNpcPatrolComponent>(TEXT("PatrolState")))
    , health(CreateDefaultSubobject<UHealthComponent>(TEXT("Health"))) {
    PrimaryActorTick.bCanEverTick = false;

    body_mesh->SetupAttachment(RootComponent);

    health->max_health = 1e9;
    health->initial_health = 1e9;

    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void ACombatDummy::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    RETURN_IF_NULLPTR(controller_class);
    if (controller_class) {
        AIControllerClass = controller_class;
    }
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
