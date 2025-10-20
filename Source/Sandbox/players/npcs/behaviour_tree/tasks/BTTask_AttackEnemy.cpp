#include "Sandbox/players/npcs/behaviour_tree/tasks/BTTask_AttackEnemy.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"

#include "Sandbox/players/common/utilities/navigation.h"
#include "Sandbox/players/npcs/interfaces/CombatActor.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

UBTTask_AttackEnemy::UBTTask_AttackEnemy() {
    NodeName = TEXT("Attack enemy");

    // This task can be aborted if higher priority behavior needs to run
    bNotifyTick = false;
    bNotifyTaskFinished = true;
}

EBTNodeResult::Type UBTTask_AttackEnemy::ExecuteTask(UBehaviorTreeComponent& owner_comp,
                                                     uint8* node_memory) {
    constexpr auto logger{NestedLogger<"ExecuteTask">()};

    INIT_PTR_OR_RETURN_VALUE(ai_controller, owner_comp.GetAIOwner(), EBTNodeResult::Failed);
    INIT_PTR_OR_RETURN_VALUE(pawn, ai_controller->GetPawn(), EBTNodeResult::Failed);

    INIT_PTR_OR_RETURN_VALUE(
        target,
        Cast<AActor>(target_actor.GetValue(owner_comp.GetBlackboardComponent())),
        EBTNodeResult::Failed);
    INIT_PTR_OR_RETURN_VALUE(combat_actor, Cast<ICombatActor>(pawn), EBTNodeResult::Failed);

    auto const attack_succeeded{combat_actor->attack_actor(target)};

    return attack_succeeded ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}
