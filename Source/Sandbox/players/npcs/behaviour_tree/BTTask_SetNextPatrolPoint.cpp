#include "Sandbox/players/npcs/behaviour_tree/BTTask_SetNextPatrolPoint.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

#include "Sandbox/pathfinding/PatrolWaypoint.h"
#include "Sandbox/players/npcs/blackboard_utils.hpp"
#include "Sandbox/players/npcs/NpcPatrolComponent.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

UBTTask_SetNextPatrolPoint::UBTTask_SetNextPatrolPoint() {
    NodeName = TEXT("Set Next Patrol Point");

    bNotifyTick = false;
    bNotifyTaskFinished = true;

    move_destination.AddVectorFilter(
        this, GET_MEMBER_NAME_CHECKED(UBTTask_SetNextPatrolPoint, move_destination));
}

EBTNodeResult::Type UBTTask_SetNextPatrolPoint::ExecuteTask(UBehaviorTreeComponent& owner_comp,
                                                            uint8* node_memory) {
    INIT_PTR_OR_RETURN_VALUE(ai_controller, owner_comp.GetAIOwner(), EBTNodeResult::Failed);
    INIT_PTR_OR_RETURN_VALUE(pawn, ai_controller->GetPawn(), EBTNodeResult::Failed);
    INIT_PTR_OR_RETURN_VALUE(
        patrol, pawn->GetComponentByClass<UNpcPatrolComponent>(), EBTNodeResult::Failed);
    INIT_PTR_OR_RETURN_VALUE(bb, owner_comp.GetBlackboardComponent(), EBTNodeResult::Failed);

    patrol->advance_to_next_point();
    INIT_PTR_OR_RETURN_VALUE(wp, patrol->get_current_point(), EBTNodeResult::Failed);

    ml::set_bb_value(*bb, move_destination.SelectedKeyName, wp->get_point());

    return EBTNodeResult::Succeeded;
}
