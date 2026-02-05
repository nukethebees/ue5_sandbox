#include "Sandbox/players/npcs/SimpleAIController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"

#include "Sandbox/players/npcs/SandboxMobInterface.h"
#include "Sandbox/utilities/macros/null_checks.hpp"

ASimpleAIController::ASimpleAIController()
    : ai_perception(CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception")))
    , sight_config(CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"))) {
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
}

void ASimpleAIController::OnPossess(APawn* InPawn) {
    Super::OnPossess(InPawn);

    RETURN_IF_NULLPTR(InPawn);
    RETURN_IF_NULLPTR(behaviour_tree_asset);

    auto* bb_ptr{Blackboard.Get()};
    UseBlackboard(behaviour_tree_asset->BlackboardAsset, bb_ptr);
    RunBehaviorTree(behaviour_tree_asset);

    auto const team_if{Cast<IGenericTeamAgentInterface>(InPawn)};
    RETURN_IF_NULLPTR(team_if);
    SetGenericTeamId(team_if->GetGenericTeamId());

    auto const mob_interface{Cast<ISandboxMobInterface>(InPawn)};
    RETURN_IF_NULLPTR(mob_interface);

    Blackboard->SetValueAsFloat("acceptable_radius", mob_interface->get_acceptable_radius());

    Blackboard->SetValueAsEnum("default_ai_state",
                               std::to_underlying(mob_interface->get_default_ai_state()));
}

void ASimpleAIController::OnUnPossess() {
    Super::OnUnPossess();
}

void ASimpleAIController::on_target_perception_updated(AActor* actor, FAIStimulus stimulus) {
    constexpr auto LOG{NestedLogger<"on_target_perception_updated">()};

    RETURN_IF_NULLPTR(actor);
    RETURN_IF_NULLPTR(Blackboard);

    static auto const target_name{TEXT("target_actor")};

    if (stimulus.WasSuccessfullySensed()) {
        auto const attitude{GetTeamAttitudeTowards(*actor)};

        if (attitude == ETeamAttitude::Hostile) {
            Blackboard->SetValueAsObject(target_name, actor);
            LOG.log_display(TEXT("Spotted: %s"), *actor->GetName());
        }
    } else {
        Blackboard->ClearValue(target_name);
        LOG.log_display(TEXT("Lost sight of: %s"), *actor->GetName());
    }
}
