#include "Sandbox/mass_entity/processors/MassBulletVisualizationProcessor.h"

#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassProcessingTypes.h"

#include "Sandbox/actors/MassBulletVisualizationActor.h"
#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"

#include "Sandbox/macros/null_checks.hpp"

void FMassBulletVisualizationExecutor::Execute(FMassExecutionContext& context) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::FMassBulletVisualizationExecutor::Execute"))

    constexpr auto executor{[](FMassExecutionContext& context, auto& Data, uint32 EntityIndex) {
        auto const viz_fragment{
            context.GetConstSharedFragment<FMassBulletVisualizationActorFragment>()};
        RETURN_IF_NULLPTR(viz_fragment.actor);

        auto const transforms{context.GetFragmentView<FMassBulletTransformFragment>()};
        auto const indices{context.GetFragmentView<FMassBulletInstanceIndexFragment>()};

        viz_fragment.actor->update_instance(indices[EntityIndex].instance_index,
                                            transforms[EntityIndex].transform);
    }};

    ForEachEntity(context, accessors, std::move(executor));
}

UMassBulletVisualizationProcessor::UMassBulletVisualizationProcessor()
    : entity_query(*this) {
    executor =
        UE::Mass::FQueryExecutor::CreateQuery<FMassBulletVisualizationExecutor>(entity_query, this);
    AutoExecuteQuery = executor;

    SetProcessingPhase(EMassProcessingPhase::FrameEnd);

    if (HasAnyFlags(RF_ClassDefaultObject)) {
        bRequiresGameThreadExecution = true;
        set_execution_flags(EProcessorExecutionFlags::All);
    }
}
