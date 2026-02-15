#include "Sandbox/players/CombatDummyController.h"

#include "BehaviorTree/BehaviorTree.h"

#include "Sandbox/environment/utilities/actor_utils.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/CombatDummy.h"
#include "Sandbox/players/TestEnemyBlackboardConstants.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

using C = TestEnemyBlackboardConstants::FName;

ACombatDummyController::ACombatDummyController() {}

void ACombatDummyController::OnPossess(APawn* pawn) {
    Super::OnPossess(pawn);

    check(behaviour_tree_asset);

    RETURN_IF_NULLPTR(pawn);
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
}

void ACombatDummyController::set_ai_state(EAIState state) {
    ai_state = state;
    set_bb_value(C::ai_state(), state);
}
