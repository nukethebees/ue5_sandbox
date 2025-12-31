#pragma once

#include "Tasks/StateTreeAITask.h"

#include "Sandbox/players/npcs/enums/AIState.h"

#include "StateTreeGetDefaultAiStateTask.generated.h"

class AActor;
class ATestEnemyController;

USTRUCT()
struct FStateTreeGetDefaultAiStateTaskData {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Context")
    ATestEnemyController* AIController{nullptr};

    UPROPERTY(VisibleAnywhere, Category = "Output")
    EAIState state;
};

USTRUCT(meta = (DisplayName = "Get Default AI State", Category = "AI"))
struct FStateTreeGetDefaultAiStateTask : public FStateTreeAITaskBase {
    GENERATED_BODY()

    using FInstanceDataType = FStateTreeGetDefaultAiStateTaskData;
    using Context = FStateTreeExecutionContext;
    using TransitionResult = FStateTreeTransitionResult;
    using RunStatus = EStateTreeRunStatus;

    FStateTreeGetDefaultAiStateTask();

    virtual UStruct const* GetInstanceDataType() const override {
        return FInstanceDataType::StaticStruct();
    }

    virtual auto EnterState(Context& Context, TransitionResult const& Transition) const
        -> RunStatus override;
};
