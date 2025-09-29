#include "Sandbox/characters/SimpleCharacter.h"

#include "Sandbox/ai_controllers/SimpleAIController.h"

ASimpleCharacter::ASimpleCharacter() {
    PrimaryActorTick.bCanEverTick = false;

    AIControllerClass = ASimpleAIController::StaticClass();
}
