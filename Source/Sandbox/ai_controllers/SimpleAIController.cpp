#include "Sandbox/ai_controllers/SimpleAIController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Sandbox/macros/null_checks.hpp"

ASimpleAIController::ASimpleAIController() {
    behavior_tree_component =
        CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorTreeComponent"));
    blackboard_component =
        CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComponent"));

    ai_perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));

    sight_config = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
    sight_config->SightRadius = 800.0f;
    sight_config->LoseSightRadius = 900.0f;
    sight_config->PeripheralVisionAngleDegrees = 270.0f;
    sight_config->SetMaxAge(5.0f);

    sight_config->DetectionByAffiliation.bDetectEnemies = true;
    sight_config->DetectionByAffiliation.bDetectFriendlies = true;
    sight_config->DetectionByAffiliation.bDetectNeutrals = true;

    ai_perception->ConfigureSense(*sight_config);
    ai_perception->SetDominantSense(sight_config->GetSenseImplementation());

    // Auto-update blackboard when target is spotted/lost
    ai_perception->OnTargetPerceptionUpdated.AddDynamic(
        this, &ASimpleAIController::on_target_perception_updated);

    SetPerceptionComponent(*ai_perception);
}

void ASimpleAIController::BeginPlay() {
    Super::BeginPlay();

    if (behavior_tree) {
        UseBlackboard(behavior_tree->BlackboardAsset, blackboard_component);
        RunBehaviorTree(behavior_tree);
    } else {
        log_warning(TEXT("behavior_tree is nullptr."));
    }
}

void ASimpleAIController::on_target_perception_updated(AActor* Actor, FAIStimulus Stimulus) {
    constexpr auto LOG{NestedLogger<"on_target_perception_updated">()};

    if (!Actor) {
        LOG.log_warning(TEXT("Actor is nullptr."));
        return;
    }
    if (!blackboard_component) {
        LOG.log_warning(TEXT("GetBlackboardComponent is nullptr."));
        return;
    }

    static auto const target_name{TEXT("target_actor")};

    if (Stimulus.WasSuccessfullySensed()) {
        blackboard_component->SetValueAsObject(target_name, Actor);
        LOG.log_display(TEXT("Spotted: %s"), *Actor->GetName());
    } else {
        blackboard_component->ClearValue(target_name);
        LOG.log_display(TEXT("Lost sight of: %s"), *Actor->GetName());
    }
}
