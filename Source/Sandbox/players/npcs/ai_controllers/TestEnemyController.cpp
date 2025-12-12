#include "Sandbox/players/npcs/ai_controllers/TestEnemyController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"

#include "Sandbox/environment/utilities/actor_utils.h"
#include "Sandbox/players/npcs/interfaces/SandboxMobInterface.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

bool is_valid_key(UBlackboardComponent& bb, FName const& name) {
    return bb.GetKeyID(name) != FBlackboard::InvalidKey;
}

ATestEnemyController::ATestEnemyController()
    : ai_perception(CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception")))
    , behavior_tree_component(
          CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorTreeComponent")))
    , blackboard_component(
          CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComponent")))
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
        this, &ATestEnemyController::on_target_perception_updated);

    SetPerceptionComponent(*ai_perception);
}

void ATestEnemyController::BeginPlay() {
    Super::BeginPlay();
}

void ATestEnemyController::OnPossess(APawn* InPawn) {
    Super::OnPossess(InPawn);

    RETURN_IF_NULLPTR(InPawn);
    RETURN_IF_NULLPTR(blackboard_component);

    auto const team_if{Cast<IGenericTeamAgentInterface>(InPawn)};
    RETURN_IF_NULLPTR(team_if);
    SetGenericTeamId(team_if->GetGenericTeamId());

    auto const mob_interface{Cast<ISandboxMobInterface>(InPawn)};
    RETURN_IF_NULLPTR(mob_interface);

    auto const behavior_tree{mob_interface->get_behaviour_tree_asset()};
    RETURN_IF_NULLPTR(behavior_tree);

    FName const acceptable_radius{TEXT("acceptable_radius")};
    FName const attack_radius{TEXT("attack_acceptable_radius")};

    check(is_valid_key(*blackboard_component, acceptable_radius));
    check(is_valid_key(*blackboard_component, attack_radius));

    blackboard_component->SetValueAsFloat(acceptable_radius,
                                          mob_interface->get_acceptable_radius());
    blackboard_component->SetValueAsFloat(attack_radius,
                                          mob_interface->get_attack_acceptable_radius());

    UseBlackboard(behavior_tree->BlackboardAsset, blackboard_component);
    RunBehaviorTree(behavior_tree);
}

void ATestEnemyController::OnUnPossess() {
    Super::OnUnPossess();

    if (behavior_tree_component) {
        behavior_tree_component->StopTree();
    }
}

void ATestEnemyController::on_target_perception_updated(AActor* actor, FAIStimulus stimulus) {
    constexpr auto LOG{NestedLogger<"on_target_perception_updated">()};

    RETURN_IF_NULLPTR(actor);
    RETURN_IF_NULLPTR(blackboard_component);

    static auto const target_name{TEXT("target_actor")};

    if (stimulus.WasSuccessfullySensed()) {
        auto const attitude{GetTeamAttitudeTowards(*actor)};

        if (attitude == ETeamAttitude::Hostile) {
            on_enemy_spotted.Broadcast();

            blackboard_component->SetValueAsObject(target_name, actor);
            LOG.log_display(TEXT("Spotted: %s"), *ml::get_best_display_name(*actor));
        }
    } else {
        if (blackboard_component->GetValueAsObject(target_name)) {
            LOG.log_display(TEXT("Lost sight of: %s"), *ml::get_best_display_name(*actor));
        }
        blackboard_component->ClearValue(target_name);
    }
}
