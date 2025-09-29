#include "Sandbox/behaviour_tree/tasks/BTTask_MoveToRandom.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
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
    TRY_INIT_BTTASK_PTR(pawn, ai_controller->GetPawn());
    TRY_INIT_BTTASK_PTR(world, ai_controller->GetWorld());
    TRY_INIT_BTTASK_PTR(nav_sys, UNavigationSystemV1::GetCurrent(world));

    auto const actor_location{pawn->GetActorLocation()};

    FNavLocation random_point{};
    bool const found{
        nav_sys->GetRandomReachablePointInRadius(actor_location, radius, random_point)};

    if (!found) {
        return EBTNodeResult::Failed;
    }

    // Move to the random point
    FAIMoveRequest move_request{random_point.Location};
    move_request.SetAcceptanceRadius(acceptable_radius);

    FPathFollowingRequestResult result{ai_controller->MoveTo(move_request)};

    if (result.Code == EPathFollowingRequestResult::RequestSuccessful) {
        return EBTNodeResult::InProgress;
    }

    return EBTNodeResult::Failed;
}
