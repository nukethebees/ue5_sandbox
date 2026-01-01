#include "Sandbox/players/npcs/behaviour_tree/tasks/BTTask_PerformLookPattern.h"

#include "AIController.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/enums.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

UBTTask_PerformLookPattern::UBTTask_PerformLookPattern() {
    // Each node instance needs its own rotation
    bCreateNodeInstance = true;
    bNotifyTick = true;
}

auto UBTTask_PerformLookPattern::ExecuteTask(UBehaviorTreeComponent& owner_comp, uint8* node_memory)
    -> EBTNodeResult::Type {

    INIT_PTR_OR_RETURN_VALUE(ai_controller, owner_comp.GetAIOwner(), EBTNodeResult::Failed);
    INIT_PTR_OR_RETURN_VALUE(pawn, ai_controller->GetPawn(), EBTNodeResult::Failed);

    move_state = ELookPatternMoveState::GoingRight;

    auto const bb_angle{half_angle_degrees.GetValue(owner_comp)};
    auto const angle{static_cast<double>(FMath::Abs(bb_angle))};
    auto const rot{FRotator{0.f, -angle, 0.f}};

    fwd_rot = pawn->GetActorRotation();
    left_rot = fwd_rot + rot;
    right_rot = fwd_rot - rot;
    tgt_rot = get_target_rotation(move_state);

    return EBTNodeResult::InProgress;
}
void UBTTask_PerformLookPattern::TickTask(UBehaviorTreeComponent& owner_comp,
                                          uint8* node_memory,
                                          float delta_seconds) {
    auto* ai_controller{owner_comp.GetAIOwner()};
    if (!ai_controller) {
        WARN_IS_FALSE(LogSandboxAI, ai_controller);
        return FinishLatentTask(owner_comp, EBTNodeResult::Failed);
    }

    auto pawn{ai_controller->GetPawn()};
    if (!pawn) {
        WARN_IS_FALSE(LogSandboxAI, pawn);
        return FinishLatentTask(owner_comp, EBTNodeResult::Failed);
    }

    auto const rate{interp_speed.GetValue(owner_comp)};
    auto const cur_rot{pawn->GetActorRotation()};
    auto const new_rot{FMath::RInterpTo(cur_rot, tgt_rot, delta_seconds, rate)};
    constexpr float equals_tolerance{0.5f};
    auto const reached_destination{
        FMath::IsNearlyEqual(new_rot.Yaw, tgt_rot.Yaw, equals_tolerance)};
    pawn->SetActorRotation(new_rot);

    UE_LOG(LogSandboxAI,
           Verbose,
           TEXT("Moving to %s\n    Cur : %s\n    Next: %s\n    Tgt : %s"),
           *ml::to_string_without_type_prefix(move_state),
           *cur_rot.ToString(),
           *new_rot.ToString(),
           *tgt_rot.ToString());

    if (reached_destination) {
        UE_LOG(LogSandboxAI, Verbose, TEXT("Reached destination"));

        switch (move_state) {
            using enum ELookPatternMoveState;
            case ReturnToCentre: {
                return FinishLatentTask(owner_comp, EBTNodeResult::Succeeded);
            }
            case GoingRight: {
                move_state = GoingLeft;
                tgt_rot = left_rot;
                break;
            }
            case GoingLeft: {
                move_state = ReturnToCentre;
                tgt_rot = fwd_rot;
                break;
            }
            default: {
                UE_LOG(LogSandboxAI,
                       Error,
                       TEXT("%s"),
                       *ml::make_unhandled_enum_case_warning(move_state));
                return FinishLatentTask(owner_comp, EBTNodeResult::Failed);
            }
        }
    }
}

auto UBTTask_PerformLookPattern::get_target_rotation(ELookPatternMoveState state) -> FRotator {
    switch (state) {
        using enum ELookPatternMoveState;
        case ReturnToCentre: {
            return fwd_rot;
        }
        case GoingRight: {
            return right_rot;
        }
        case GoingLeft: {
            return left_rot;
        }
        default: {
            UE_LOG(LogSandboxAI, Error, TEXT("%s"), *ml::make_unhandled_enum_case_warning(state));
            break;
        }
    }

    return FRotator::ZeroRotator;
}
