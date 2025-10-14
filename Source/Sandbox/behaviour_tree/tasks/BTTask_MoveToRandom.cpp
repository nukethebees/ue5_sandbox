#include "Sandbox/behaviour_tree/tasks/BTTask_MoveToRandom.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"

#include "Sandbox/utilities/navigation.h"

#include "Sandbox/macros/null_checks.hpp"

UBTTask_MoveToRandom::UBTTask_MoveToRandom() {
    NodeName = TEXT("Move To Random");

    // This task can be aborted if higher priority behavior needs to run
    bNotifyTick = false;
    bNotifyTaskFinished = true;
}

EBTNodeResult::Type UBTTask_MoveToRandom::ExecuteTask(UBehaviorTreeComponent& owner_comp,
                                                      uint8* node_memory) {
    constexpr auto logger{NestedLogger<"ExecuteTask">()};

    INIT_PTR_OR_RETURN_VALUE(ai_controller, owner_comp.GetAIOwner(), EBTNodeResult::Failed);
    INIT_PTR_OR_RETURN_VALUE(pawn, ai_controller->GetPawn(), EBTNodeResult::Failed);

    auto const random_point{ml::get_random_nav_point(pawn, radius.GetValue(owner_comp))};
    RETURN_VALUE_IF_FALSE(random_point, EBTNodeResult::Failed);

    FAIMoveRequest move_request{random_point->Location};
    move_request.SetAcceptanceRadius(acceptable_radius.GetValue(owner_comp));

    auto const result{ai_controller->MoveTo(move_request)};
    if (result.Code == EPathFollowingRequestResult::RequestSuccessful) {
        return EBTNodeResult::Succeeded;
    }

    logger.log_warning(TEXT("Task failed."));
    return EBTNodeResult::Failed;
}
