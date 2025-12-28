#include "Sandbox/players/npcs/ai_controllers/CombatDummyController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"

#include "Sandbox/environment/utilities/actor_utils.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/npcs/characters/CombatDummy.h"
#include "Sandbox/players/npcs/data/TestEnemyBlackboardConstants.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

using C = TestEnemyBlackboardConstants::FName;

ACombatDummyController::ACombatDummyController()
    : behavior_tree_component(
          CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorTreeComponent"))) {}

void ACombatDummyController::OnPossess(APawn* pawn) {
    Super::OnPossess(pawn);

    check(behavior_tree_component);
    check(behaviour_tree_asset);

    RETURN_IF_NULLPTR(pawn);
    RETURN_IF_NULLPTR(behavior_tree_component);
    RETURN_IF_NULLPTR(behaviour_tree_asset);

    auto const team_if{Cast<IGenericTeamAgentInterface>(pawn)};
    RETURN_IF_NULLPTR(team_if);
    SetGenericTeamId(team_if->GetGenericTeamId());

    auto* bb_ptr{Blackboard.Get()};
    UseBlackboard(behaviour_tree_asset->BlackboardAsset, bb_ptr);
    RunBehaviorTree(behaviour_tree_asset);

    TRY_INIT_PTR(dummy, Cast<ACombatDummy>(pawn));
    set_ai_state(dummy->get_default_ai_state());
}
void ACombatDummyController::OnUnPossess() {
    Super::OnUnPossess();

    if (behavior_tree_component) {
        behavior_tree_component->StopTree();
    }
}

void ACombatDummyController::set_ai_state(EAIState state) {
    ai_state = state;
    set_bb_value(C::ai_state(), state);
}
