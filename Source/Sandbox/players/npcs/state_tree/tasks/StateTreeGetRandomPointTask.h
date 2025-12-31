#pragma once

#include "Tasks/StateTreeAITask.h"

#include "StateTreeGetRandomPointTask.generated.h"

class AActor;

USTRUCT()
struct FStateTreeGetRandomPointTaskInstanceData {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Context")
    AActor* Actor{nullptr};

    UPROPERTY(EditAnywhere, Category = "Parameter")
    float distance{5000.f};

    UPROPERTY(VisibleAnywhere, Category = "Output")
    FVector random_point{FVector::Zero()};
};

USTRUCT(meta = (DisplayName = "Get Random Point", Category = "AI|Action"))
struct FStateTreeGetRandomPointTask : public FStateTreeAITaskBase {
    GENERATED_BODY()

    using FInstanceDataType = FStateTreeGetRandomPointTaskInstanceData;
    using Context = FStateTreeExecutionContext;
    using TransitionResult = FStateTreeTransitionResult;
    using RunStatus = EStateTreeRunStatus;

    FStateTreeGetRandomPointTask();

    virtual UStruct const* GetInstanceDataType() const override {
        return FInstanceDataType::StaticStruct();
    }

    virtual auto EnterState(Context& Context, TransitionResult const& Transition) const
        -> RunStatus override;
};
