#include "Sandbox/mass_entity/processors/MassBulletMovementProcessor.h"

#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"

void FMassBulletMovementExecutor::Execute(FMassExecutionContext& context) {
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

    ForEachEntityChunk(context, accessors, std::move(executor));
}

UMassBulletMovementProcessor::UMassBulletMovementProcessor()
    : entity_query(*this) {
    executor =
        UE::Mass::FQueryExecutor::CreateQuery<FMassBulletMovementExecutor>(entity_query, this);
    AutoExecuteQuery = executor;

    SetProcessingPhase(EMassProcessingPhase::PrePhysics);

    if (HasAnyFlags(RF_ClassDefaultObject)) {
        set_execution_flags(EProcessorExecutionFlags::All);
        bRequiresGameThreadExecution = false;
    }
}
