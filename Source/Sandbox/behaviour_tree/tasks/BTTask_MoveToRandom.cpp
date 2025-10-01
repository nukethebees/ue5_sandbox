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

EBTNodeResult::Type UBTTask_MoveToRandom::ExecuteTask(UBehaviorTreeComponent& OwnerComp,
                                                      uint8* NodeMemory) {
    TRY_INIT_BTTASK_PTR(ai_controller, OwnerComp.GetAIOwner());
    auto const random_point{ml::get_random_nav_point(ai_controller->GetPawn(), radius)};
    if (!random_point) {
        UE_LOG(LogTemp, Warning, TEXT("No point found."));
        return EBTNodeResult::Failed;
    }

    FAIMoveRequest move_request{random_point->Location};
    move_request.SetAcceptanceRadius(acceptable_radius);

    auto const result{ai_controller->MoveTo(move_request)};
    if (result.Code == EPathFollowingRequestResult::RequestSuccessful) {
        return EBTNodeResult::Succeeded;
    }

    UE_LOG(LogTemp, Warning, TEXT("Task failed."));
    return EBTNodeResult::Failed;
}
