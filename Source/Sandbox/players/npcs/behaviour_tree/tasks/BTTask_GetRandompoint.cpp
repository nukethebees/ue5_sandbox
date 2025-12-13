#include "Sandbox/players/npcs/behaviour_tree/tasks/BTTask_GetRandomPoint.h"

#include "AIController.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/common/utilities/navigation.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

UBTTask_GetRandomPoint::UBTTask_GetRandomPoint() {
    NodeName = TEXT("Get Random Point");

    bNotifyTick = false;
    bNotifyTaskFinished = true;
}

EBTNodeResult::Type UBTTask_GetRandomPoint::ExecuteTask(UBehaviorTreeComponent& owner_comp,
                                                        uint8* node_memory) {
    INIT_PTR_OR_RETURN_VALUE(ai_controller, owner_comp.GetAIOwner(), EBTNodeResult::Failed);
    INIT_PTR_OR_RETURN_VALUE(pawn, ai_controller->GetPawn(), EBTNodeResult::Failed);

    auto const bb_travel_radius{travel_radius.GetValue(owner_comp)};
    auto const random_point{ml::get_random_nav_point(pawn, bb_travel_radius)};
    RETURN_VALUE_IF_FALSE(random_point, EBTNodeResult::Failed);

    INIT_PTR_OR_RETURN_VALUE(bb, owner_comp.GetBlackboardComponent(), EBTNodeResult::Failed);
    bb->SetValueAsVector(target_location.SelectedKeyName, random_point->Location);
    return EBTNodeResult::Succeeded;
}
