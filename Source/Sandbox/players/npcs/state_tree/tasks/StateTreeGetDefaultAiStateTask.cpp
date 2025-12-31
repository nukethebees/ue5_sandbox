#include "Sandbox/players/npcs/state_tree/tasks/StateTreeGetDefaultAiStateTask.h"

#include "StateTreeExecutionContext.h"

#include "Sandbox/players/common/utilities/navigation.h"
#include "Sandbox/players/npcs/ai_controllers/TestEnemyController.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

FStateTreeGetDefaultAiStateTask::FStateTreeGetDefaultAiStateTask() {
    bShouldCallTick = false;
    bShouldCopyBoundPropertiesOnTick = false;
    bShouldCopyBoundPropertiesOnExitState = false;
    bShouldAffectTransitions = false;
    bConsideredForScheduling = false;
}

auto FStateTreeGetDefaultAiStateTask::EnterState(Context& context,
                                                 TransitionResult const& transition) const
    -> RunStatus {
    auto& instance_data{context.GetInstanceData(*this)};
    instance_data.state = instance_data.AIController->get_ai_state();

    return RunStatus::Succeeded;
}
