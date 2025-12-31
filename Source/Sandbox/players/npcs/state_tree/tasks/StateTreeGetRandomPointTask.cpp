#include "Sandbox/players/npcs/state_tree/tasks/StateTreeGetRandomPointTask.h"

#include "StateTreeExecutionContext.h"

#include "Sandbox/players/common/utilities/navigation.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

using Task = FStateTreeGetRandomPointTask;

FStateTreeGetRandomPointTask::FStateTreeGetRandomPointTask() {
    bShouldCallTick = false;
    bShouldCopyBoundPropertiesOnTick = false;
    bShouldCopyBoundPropertiesOnExitState = false;
    bShouldAffectTransitions = false;
    bConsideredForScheduling = false;
}

auto Task::EnterState(Context& context, TransitionResult const& transition) const -> RunStatus {
    auto& instance_data{context.GetInstanceData(*this)};

    RETURN_VALUE_IF_NULLPTR(instance_data.Actor, RunStatus::Failed);

    auto const random_point{ml::get_random_nav_point(instance_data.Actor, instance_data.distance)};
    RETURN_VALUE_IF_FALSE(random_point, RunStatus::Failed);

    instance_data.random_point = *random_point;

    return RunStatus::Running;
}
