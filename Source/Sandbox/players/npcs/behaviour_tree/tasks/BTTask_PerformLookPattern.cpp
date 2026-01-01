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
    pawn = ai_controller->GetPawn();
    if (!pawn) {
        WARN_IS_FALSE(LogSandboxAI, pawn);
        return EBTNodeResult::Failed;
    }
    move_state = ELookPatternMoveState::GoingRight;

    auto const bb_angle{half_angle_degrees.GetValue(owner_comp)};
    auto const angle{static_cast<double>(FMath::ClampAngle(FMath::Abs(bb_angle), 0.01f, 179.99))};

    auto const rot_left{FRotator{0.f, -angle, 0.f}};
    auto const rot_right{FRotator{0.f, angle, 0.f}};
    FQuat delta_left{FQuat::MakeFromRotator(rot_left)};
    FQuat delta_right{FQuat::MakeFromRotator(rot_right)};

    actor_fwd = pawn->GetActorQuat();
    actor_left = actor_fwd * delta_left;
    actor_right = actor_fwd * delta_right;
    start_rotation = actor_fwd;
    target = get_target_rotation(move_state);
    progress = 0.0f;

    UE_LOG(LogSandboxAI,
           Verbose,
           TEXT("Starting UBTTask_PerformLookPattern Fwd: %s, Left: %s, Right:%s "
                "\n    Fwd  : %s\n    Left : %s\n    Right: %s"),
           *actor_fwd.Rotator().ToCompactString(),
           *actor_left.Rotator().ToCompactString(),
           *actor_right.Rotator().ToCompactString(),
           *actor_fwd.ToString(),
           *actor_left.ToString(),
           *actor_right.ToString());

    return EBTNodeResult::InProgress;
}
void UBTTask_PerformLookPattern::TickTask(UBehaviorTreeComponent& owner_comp,
                                          uint8* node_memory,
                                          float delta_seconds) {
    check(pawn);

    auto const rate{interp_speed.GetValue(owner_comp)};
    constexpr float progress_end{1.0f};
    progress = FMath::FInterpTo(progress, progress_end, delta_seconds, rate);

    auto const new_rot{FQuat::SlerpFullPath_NotNormalized(start_rotation, target, progress)};

    constexpr float equals_tolerance{0.1f};
    auto const angular_distance{FMath::RadiansToDegrees(new_rot.AngularDistance(target))};
    auto const reached_destination{FMath::IsNearlyEqual(progress, progress_end, 0.01)};
    pawn->SetActorRotation(new_rot);

    UE_LOG(LogSandboxAI,
           VeryVerbose,
           TEXT("Moving to %s: Next: %s, Tgt: %s, Progress: %.2f"),
           *ml::to_string_without_type_prefix(move_state),
           *new_rot.Rotator().ToCompactString(),
           *target.Rotator().ToCompactString(),
           progress);

    if (reached_destination) {
        UE_LOG(LogSandboxAI,
               Verbose,
               TEXT("Reached: %s"),
               *ml::to_string_without_type_prefix(move_state));

        switch (move_state) {
            using enum ELookPatternMoveState;
            case ReturnToCentre: {
                UE_LOG(LogSandboxAI, Verbose, TEXT("UBTTask_PerformLookPattern: Finished."));
                return FinishLatentTask(owner_comp, EBTNodeResult::Succeeded);
            }
            case GoingRight: {
                move_state = GoingLeft;
                set_new_target(actor_left);
                break;
            }
            case GoingLeft: {
                move_state = ReturnToCentre;
                set_new_target(actor_fwd);
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

auto UBTTask_PerformLookPattern::get_target_rotation(ELookPatternMoveState state) -> FQuat {
    switch (state) {
        using enum ELookPatternMoveState;
        case ReturnToCentre: {
            return actor_fwd;
        }
        case GoingRight: {
            return actor_right;
        }
        case GoingLeft: {
            return actor_left;
        }
        default: {
            UE_LOG(LogSandboxAI, Error, TEXT("%s"), *ml::make_unhandled_enum_case_warning(state));
            break;
        }
    }

    return FQuat::Identity;
}
void UBTTask_PerformLookPattern::set_new_target(FQuat new_tgt) {
    start_rotation = target;
    target = new_tgt;
    progress = 0.0f;
}
