#include "Sandbox/mass_entity/processors/MassBulletMovementProcessor.h"

#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"

void FMassBulletMovementExecutor::Execute(FMassExecutionContext& context) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::FMassBulletMovementExecutor::Execute"))

    constexpr auto executor{[](FMassExecutionContext& context, auto& Data, uint32 EntityIndex) {
        auto const delta_time{context.GetDeltaTimeSeconds()};
        auto const transforms{context.GetMutableFragmentView<FMassBulletTransformFragment>()};
        auto const velocities{context.GetFragmentView<FMassBulletVelocityFragment>()};

        auto const displacement{velocities[EntityIndex].velocity * delta_time};
        transforms[EntityIndex].transform.AddToTranslation(displacement);
    }};

    ForEachEntity(context, accessors, std::move(executor));
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
    }
}
