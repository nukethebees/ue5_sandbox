#include "Sandbox/mass_entity/processors/MassBulletMovementProcessor.h"

#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"

void FMassBulletMovementExecutor::Execute(FMassExecutionContext& context) {
#define EXECUTION_BRANCH 0
#if (EXECUTION_BRANCH == 0)
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::FMassBulletMovementExecutor::Execute"))

    constexpr auto executor{[](FMassExecutionContext& context, auto& Data, uint32 EntityIndex) {
        auto const delta_time{context.GetDeltaTimeSeconds()};
        auto const transforms{context.GetMutableFragmentView<FMassBulletTransformFragment>()};
        auto const velocities{context.GetFragmentView<FMassBulletVelocityFragment>()};

        auto const displacement{velocities[EntityIndex].velocity * delta_time};
        transforms[EntityIndex].transform.AddToTranslation(displacement);
    }};

    ForEachEntity(context, accessors, std::move(executor));
#elif (EXECUTION_BRANCH == 1)
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::FMassBulletMovementExecutor::Execute"))

    constexpr auto executor{[](FMassExecutionContext& context, auto& Data, uint32 EntityIndex) {
        auto const delta_time{context.GetDeltaTimeSeconds()};
        auto const transforms{context.GetMutableFragmentView<FMassBulletTransformFragment>()};
        auto const velocities{context.GetFragmentView<FMassBulletVelocityFragment>()};

        auto const displacement{velocities[EntityIndex].velocity * delta_time};
        transforms[EntityIndex].transform.AddToTranslation(displacement);
    }};

    ParallelForEachEntity(context, accessors, std::move(executor));
#elif (EXECUTION_BRANCH == 2)
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::FMassBulletMovementExecutor::Execute"))

    constexpr auto executor{[](FMassExecutionContext& context, auto& Data) {
        auto const n{context.GetNumEntities()};
        auto const delta_time{context.GetDeltaTimeSeconds()};

        auto const transforms{context.GetMutableFragmentView<FMassBulletTransformFragment>()};
        auto const velocities{context.GetFragmentView<FMassBulletVelocityFragment>()};

        for (int32 i{0}; i < n; ++i) {
            auto const displacement{velocities[i].velocity * delta_time};
            transforms[i].transform.AddToTranslation(displacement);
        }
    }};

    ParallelForEachEntityChunk(context, accessors, std::move(executor));
#else
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::FMassBulletMovementExecutor::Execute"))

    constexpr auto executor{[](FMassExecutionContext& context, auto& Data) {
        auto const n{context.GetNumEntities()};
        auto const delta_time{context.GetDeltaTimeSeconds()};

        for (int32 i{0}; i < n; ++i) {
            Data.Get<FMassBulletTransformFragment>()[i].transform.AddToTranslation(
                Data.Get<FMassBulletVelocityFragment>()[i].velocity * delta_time);
        }
    }};

    ParallelForEachEntityChunk(context, accessors, std::move(executor));
#endif
}

UMassBulletMovementProcessor::UMassBulletMovementProcessor()
    : entity_query(*this) {
    executor =
        UE::Mass::FQueryExecutor::CreateQuery<FMassBulletMovementExecutor>(entity_query, this);
    AutoExecuteQuery = executor;

    if (HasAnyFlags(RF_ClassDefaultObject)) {
        SetShouldAutoRegisterWithGlobalList(true);
        ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
        set_execution_flags(EProcessorExecutionFlags::All);
        bRequiresGameThreadExecution = false;
    }
}
