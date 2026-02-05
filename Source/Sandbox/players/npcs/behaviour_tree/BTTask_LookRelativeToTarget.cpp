#include "Sandbox/players/npcs/behaviour_tree/BTTask_LookRelativeToTarget.h"

#include "AIController.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/geometry.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

UBTTask_LookRelativeToTarget::UBTTask_LookRelativeToTarget() {
    bCreateNodeInstance = false;
    bNotifyTick = false;

    target_key.AddObjectFilter(this,
                               GET_MEMBER_NAME_CHECKED(UBTTask_LookRelativeToTarget, target_key),
                               AActor::StaticClass());
    target_key.AddVectorFilter(this,
                               GET_MEMBER_NAME_CHECKED(UBTTask_LookRelativeToTarget, target_key));
}

auto UBTTask_LookRelativeToTarget::ExecuteTask(UBehaviorTreeComponent& owner_comp,
                                               uint8* node_memory) -> EBTNodeResult::Type {

    INIT_PTR_OR_RETURN_VALUE(bb, owner_comp.GetBlackboardComponent(), EBTNodeResult::Failed);
    INIT_PTR_OR_RETURN_VALUE(ai_controller, owner_comp.GetAIOwner(), EBTNodeResult::Failed);
    INIT_PTR_OR_RETURN_VALUE(pawn, ai_controller->GetPawn(), EBTNodeResult::Failed);

    FVector target{FVector::ZeroVector};
    auto const key_id{target_key.GetSelectedKeyID()};

    using BB_Obj = UBlackboardKeyType_Object;
    using BB_Vec = UBlackboardKeyType_Vector;
    if (target_key.SelectedKeyType == BB_Obj::StaticClass()) {
        INIT_PTR_OR_RETURN_VALUE(key_value, bb->GetValue<BB_Obj>(key_id), EBTNodeResult::Failed);
        INIT_PTR_OR_RETURN_VALUE(target_actor, Cast<AActor>(key_value), EBTNodeResult::Failed);
        target = target_actor->GetActorLocation();
    } else if (target_key.SelectedKeyType == BB_Vec::StaticClass()) {
        auto const key_value{bb->GetValue<BB_Vec>(key_id)};
        target = key_value;
    } else {
        UE_LOG(LogSandboxAI, Warning, TEXT("No target key value."));
        return EBTNodeResult::Failed;
    }

    auto const direction{ml::get_norm_vector_to_actor(*pawn, target).Rotation()};
    FRotator rotator{0.0f, angle_deg.GetValue(owner_comp), 0.0f};
    FRotator const new_direction{0.f, (direction + rotator).Yaw, 0.f};

    UE_LOG(LogSandboxAI,
           Verbose,
           TEXT("UBTTask_LookRelativeToTarget\n    Pawn: %s\n    target: %s\n    dir: %s\n    rot: "
                "%s\n    new rot: %s"),
           *pawn->GetActorLocation().ToCompactString(),
           *target.ToCompactString(),
           *direction.ToCompactString(),
           *rotator.ToCompactString(),
           *new_direction.ToCompactString());

    pawn->SetActorRotation(new_direction);

    return EBTNodeResult::Succeeded;
}
void UBTTask_LookRelativeToTarget::InitializeFromAsset(UBehaviorTree& Asset) {
    Super::InitializeFromAsset(Asset);

    if (auto const* bb{GetBlackboardAsset()}) {
        target_key.ResolveSelectedKey(*bb);
    } else {
        target_key.InvalidateResolvedKey();
    }
}
