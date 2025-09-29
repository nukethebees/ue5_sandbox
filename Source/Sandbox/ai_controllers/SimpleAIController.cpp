#include "Sandbox/ai_controllers/SimpleAIController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Sandbox/macros/null_checks.hpp"

ASimpleAIController::ASimpleAIController() {
    behavior_tree_component =
        CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorTreeComponent"));
    blackboard_component =
        CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComponent"));
    ai_perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
}

void ASimpleAIController::BeginPlay() {
    Super::BeginPlay();

    if (behavior_tree) {
        RunBehaviorTree(behavior_tree);
    } else {
        log_warning(TEXT("behavior_tree is nullptr."));
    }
}
