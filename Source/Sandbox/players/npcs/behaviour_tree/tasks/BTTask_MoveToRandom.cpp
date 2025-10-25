#include "Sandbox/players/npcs/behaviour_tree/tasks/BTTask_MoveToRandom.h"

#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"

#include "Sandbox/environment/utilities/actor_utils.h"
#include "Sandbox/players/common/utilities/navigation.h"
#include "Sandbox/players/npcs/ai_controllers/TestEnemyController.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

UBTTask_MoveToRandom::UBTTask_MoveToRandom() {
    NodeName = TEXT("Move To Random");

    // This task can be aborted if higher priority behavior needs to run
    bNotifyTick = false;
    bNotifyTaskFinished = true;
    bCreateNodeInstance = true; // Needed for binding delegates
}

EBTNodeResult::Type UBTTask_MoveToRandom::ExecuteTask(UBehaviorTreeComponent& owner_comp,
                                                      uint8* node_memory) {
    constexpr auto logger{NestedLogger<"ExecuteTask">()};

    INIT_PTR_OR_RETURN_VALUE(ai_controller, owner_comp.GetAIOwner(), EBTNodeResult::Failed);
    INIT_PTR_OR_RETURN_VALUE(pawn, ai_controller->GetPawn(), EBTNodeResult::Failed);

    auto const bb_travel_radius{travel_radius.GetValue(owner_comp)};
    auto const random_point{ml::get_random_nav_point(pawn, bb_travel_radius)};
    RETURN_VALUE_IF_FALSE(random_point, EBTNodeResult::Failed);

    FAIMoveRequest move_request{random_point->Location};
    auto const bb_acceptable_radius{acceptable_radius.GetValue(owner_comp)};
    move_request.SetAcceptanceRadius(bb_acceptable_radius);

    auto const result{ai_controller->MoveTo(move_request)};

    switch (result.Code) {
        case EPathFollowingRequestResult::RequestSuccessful: {
            ai_controller->ReceiveMoveCompleted.AddDynamic(
                this, &UBTTask_MoveToRandom::on_move_completed);

            auto const bb_interrupt_for_enemy{interrupt_for_enemy.GetValue(owner_comp)};
            if (bb_interrupt_for_enemy) {
                if (auto* test_enemy_controller{Cast<ATestEnemyController>(ai_controller)}) {
                    test_enemy_controller->on_enemy_spotted.AddUObject(
                        this, &UBTTask_MoveToRandom::on_enemy_spotted);
                }
            }

            return EBTNodeResult::InProgress;
        }
        case EPathFollowingRequestResult::AlreadyAtGoal: {
            return EBTNodeResult::Succeeded;
        }
        default: {
            break;
        }
    }

    logger.log_warning(TEXT("Task failed for actor: %s"),
                       *ml::get_best_display_name(*ai_controller));
    return EBTNodeResult::Failed;
}
void UBTTask_MoveToRandom::on_move_completed(FAIRequestID RequestID,
                                             EPathFollowingResult::Type path_following_result) {
    constexpr auto logger{NestedLogger<"on_move_completed">()};

    TRY_INIT_PTR(outer, GetOuter());
    TRY_INIT_PTR(bt_comp, Cast<UBehaviorTreeComponent>(outer));

    if (auto* ai_controller{bt_comp->GetAIOwner()}) {
        ai_controller->ReceiveMoveCompleted.RemoveAll(this);
    } else {
        logger.log_warning(TEXT("ai_controller is nullptr"));
    }

    EBTNodeResult::Type task_result{};

    switch (path_following_result) {
        case EPathFollowingResult::Success: {
            task_result = EBTNodeResult::Succeeded;
            break;
        }
        default: {
            task_result = EBTNodeResult::Failed;
            break;
        }
    }

    FinishLatentTask(*bt_comp, task_result);
}
void UBTTask_MoveToRandom::on_enemy_spotted() {
    constexpr auto logger{NestedLogger<"on_enemy_spotted">()};

    TRY_INIT_PTR(outer, GetOuter());
    TRY_INIT_PTR(bt_comp, Cast<UBehaviorTreeComponent>(outer));

    auto* ai_ctrl{bt_comp->GetAIOwner()};
    if (ai_ctrl) {
        if (auto* test_enemy_controller{Cast<ATestEnemyController>(ai_ctrl)}) {
            test_enemy_controller->on_enemy_spotted.RemoveAll(this);
        }
    }

    logger.log_verbose(TEXT("Enemy spotted, aborting move."));
    FinishLatentTask(*bt_comp, EBTNodeResult::Aborted);
}
