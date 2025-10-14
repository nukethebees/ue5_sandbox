#include "Sandbox/characters/SimpleCharacter.h"

#include "Sandbox/actor_components/HealthComponent.h"
#include "Sandbox/ai_controllers/SimpleAIController.h"

ASimpleCharacter::ASimpleCharacter()
    : health(CreateDefaultSubobject<UHealthComponent>(TEXT("Health"))) {
    PrimaryActorTick.bCanEverTick = false;

    AutoPossessAI = EAutoPossessAI::Spawned;
    if (controller_class) {
        AIControllerClass = controller_class;
    } else {
        AIControllerClass = ASimpleAIController::StaticClass();
    }
}

void ASimpleCharacter::BeginPlay() {
    Super::BeginPlay();

    if (!Controller && AIControllerClass) {
        SpawnDefaultController();
    }
}

void ASimpleCharacter::handle_death() {
    SetLifeSpan(0.1f);
}
