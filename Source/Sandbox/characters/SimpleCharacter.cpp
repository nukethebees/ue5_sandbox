#include "Sandbox/characters/SimpleCharacter.h"

#include "Sandbox/ai_controllers/SimpleAIController.h"

ASimpleCharacter::ASimpleCharacter() {
    PrimaryActorTick.bCanEverTick = true;

    AIControllerClass = ASimpleAIController::StaticClass();
}
