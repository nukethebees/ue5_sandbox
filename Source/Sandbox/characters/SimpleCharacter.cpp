#include "Sandbox/characters/SimpleCharacter.h"

#include "Sandbox/actor_components/HealthComponent.h"
#include "Sandbox/ai_controllers/SimpleAIController.h"

ASimpleCharacter::ASimpleCharacter()
    : health(CreateDefaultSubobject<UHealthComponent>(TEXT("Health"))) {
    PrimaryActorTick.bCanEverTick = false;

    AIControllerClass = ASimpleAIController::StaticClass();
}
void ASimpleCharacter::handle_death() {
    SetLifeSpan(0.1f);
}
