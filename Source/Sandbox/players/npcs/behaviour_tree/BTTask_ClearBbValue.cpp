#include "Sandbox/players/npcs/behaviour_tree/BTTask_ClearBbValue.h"

#include "BehaviorTree/BlackboardComponent.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

UBTTask_ClearBbValue::UBTTask_ClearBbValue() {
    NodeName = TEXT("Clear Blackboard Value");

    bNotifyTick = false;
    bNotifyTaskFinished = true;
}

EBTNodeResult::Type UBTTask_ClearBbValue::ExecuteTask(UBehaviorTreeComponent& owner_comp,
                                                      uint8* node_memory) {
    INIT_PTR_OR_RETURN_VALUE(bb, owner_comp.GetBlackboardComponent(), EBTNodeResult::Failed);
    bb->ClearValue(bb_key.SelectedKeyName);

    return EBTNodeResult::Succeeded;
}
